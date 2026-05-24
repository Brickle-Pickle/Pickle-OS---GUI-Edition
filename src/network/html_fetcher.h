#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// HtmlFetcher - Stream-based HTTP/HTTPS fetcher with built-in HTML stripper.
class HtmlFetcher {
public:
    struct Result {
        bool success = false;
        int httpCode = 0;
        String text;
        String error;
        String finalUrl;
    };

    static Result fetch(const String& url, size_t maxChars = 3000) {
        Result r;
        r.finalUrl = url;

        String target = url;
        const int MAX_REDIRECTS = 5;

        for (int redirect = 0; redirect <= MAX_REDIRECTS; redirect++) {
            HTTPClient http;
            http.setTimeout(15000);
            http.setConnectTimeout(8000);
            http.setReuse(false);
            http.addHeader("Accept-Encoding", "identity");
            http.addHeader("Accept", "text/html,text/plain;q=0.9");
            http.addHeader("User-Agent", "PickleOS/1.0 (ESP32; text-browser)");

            bool began = false;
            WiFiClientSecure secureClient;
            if (target.startsWith("https://")) {
                secureClient.setInsecure();
                began = http.begin(secureClient, target);
            } else {
                began = http.begin(target);
            }

            if (!began) {
                r.error = "Failed to parse URL";
                http.end();
                return r;
            }

            int code = http.GET();
            r.httpCode = code;

            if (code == HTTP_CODE_MOVED_PERMANENTLY ||
                code == HTTP_CODE_FOUND ||
                code == 307 || code == 308) {
                String location = http.getLocation();
                http.end();
                if (location.length() == 0) {
                    r.error = "Redirect with no Location header";
                    return r;
                }
                if (location.startsWith("/")) {
                    int slashPos = target.indexOf('/', 8);
                    if (slashPos > 0) {
                        target = target.substring(0, slashPos) + location;
                    } else {
                        target = target + location;
                    }
                } else {
                    target = location;
                }
                r.finalUrl = target;
                continue;
            }

            if (code <= 0) {
                r.error = HTTPClient::errorToString(code);
                http.end();
                return r;
            }

            if (code != HTTP_CODE_OK) {
                r.error = "HTTP " + String(code);
                http.end();
                return r;
            }

            WiFiClient* stream = http.getStreamPtr();
            if (!stream) {
                r.error = "No stream";
                http.end();
                return r;
            }

            r.text = _stripHtml(stream, maxChars);
            r.success = true;
            http.end();
            return r;
        }

        r.error = "Too many redirects";
        return r;
    }

private:
    // Streaming state machine. Separates head and body processing:
    // - In head: captures <title> and <meta name="description"> only.
    // - In body: strips all tags and emits visible text.
    // When body text is empty (JS-rendered SPA), composes a fallback card
    // from the captured title and description so the screen is never blank.
    static String _stripHtml(WiFiClient* stream, size_t maxChars) {
        String body;
        body.reserve(512);
        String title;
        String description;

        bool inTag = false;
        bool inScript = false;
        bool inStyle = false;
        bool inHead = true;
        bool inTitle = false;
        bool lastWasSpace = true;

        String tagBuf;
        String entityBuf;
        bool inEntity = false;

        tagBuf.reserve(256);
        entityBuf.reserve(12);

        unsigned long deadline = millis() + 12000;

        while (millis() < deadline) {
            if (!stream->available()) {
                if (!stream->connected()) break;
                delay(1);
                continue;
            }

            char c = (char)stream->read();

            // Entity decoder
            if (inEntity) {
                if (c == ';') {
                    inEntity = false;
                    char decoded = _decodeEntity(entityBuf);
                    entityBuf = "";
                    if (decoded != 0) {
                        if (inTitle && title.length() < 80) {
                            title += decoded;
                        } else if (!inHead && !inTag && !inScript && !inStyle) {
                            if (body.length() < maxChars) {
                                body += decoded;
                                lastWasSpace = (decoded == ' ');
                            }
                        }
                    }
                    continue;
                } else if (entityBuf.length() < 10) {
                    entityBuf += c;
                    continue;
                } else {
                    inEntity = false;
                    entityBuf = "";
                }
            }

            if (c == '&' && !inTag) {
                inEntity = true;
                entityBuf = "";
                continue;
            }

            if (c == '<') {
                inTag = true;
                tagBuf = "";
                continue;
            }

            if (c == '>') {
                inTag = false;
                String tag = tagBuf;
                tag.toLowerCase();
                String savedTag = tag;
                tagBuf = "";

                _handleTag(tag, savedTag, inScript, inStyle, inHead, inTitle, description);

                // Block-level tags produce a newline in body output
                if (!inHead && _isBlockTag(tag) && body.length() < maxChars) {
                    if (!lastWasSpace) {
                        body += '\n';
                        lastWasSpace = true;
                    }
                }
                continue;
            }

            if (inTag) {
                if (tagBuf.length() < 256) tagBuf += c;
                continue;
            }

            // Capture title text
            if (inTitle) {
                if (title.length() < 80) title += c;
                continue;
            }

            // Skip all other head content
            if (inHead) continue;

            if (inScript || inStyle) continue;

            // Collapse whitespace in body
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';

            if (c == ' ') {
                if (!lastWasSpace && body.length() < maxChars) {
                    body += ' ';
                    lastWasSpace = true;
                }
                continue;
            }

            if (body.length() >= maxChars) break;

            body += c;
            lastWasSpace = false;
        }

        body.trim();
        title.trim();
        description.trim();

        // Body has readable content: prepend the title and return
        if (body.length() > 20) {
            String out;
            if (title.length() > 0) {
                out = "[ " + title + " ]\n\n";
            }
            out += body;
            return out;
        }

        // JS-rendered page: body was empty. Build a fallback card.
        String fallback;
        if (title.length() > 0) {
            fallback = "[ " + title + " ]\n\n";
        }
        if (description.length() > 0) {
            fallback += description + "\n\n";
        }
        fallback += "(This page requires JavaScript to render its content. "
                    "Only static HTML is visible in text mode.)";
        return fallback;
    }

    // Updates parser flags and extracts meta description from the current tag.
    // tag: lowercase version. savedTag: original case (for attribute parsing).
    static void _handleTag(const String& tag, const String& savedTag,
                            bool& inScript, bool& inStyle,
                            bool& inHead, bool& inTitle, String& description) {
        if (tag.startsWith("script") && !tag.startsWith("script/")) {
            inScript = true; return;
        }
        if (tag.startsWith("/script")) { inScript = false; return; }
        if (tag.startsWith("style") && !tag.startsWith("style/")) {
            inStyle = true; return;
        }
        if (tag.startsWith("/style")) { inStyle = false; return; }

        if (tag.startsWith("body")) { inHead = false; return; }
        if (tag.startsWith("/head")) { inHead = false; return; }

        if (tag == "title") { inTitle = true; return; }
        if (tag.startsWith("/title")) { inTitle = false; return; }

        // Extract <meta name="description" content="...">
        if (tag.startsWith("meta") && description.length() == 0) {
            if (tag.indexOf("name=\"description\"") >= 0 ||
                tag.indexOf("name='description'") >= 0) {
                description = _extractAttr(savedTag, "content");
                if (description.length() > 200) description.remove(200);
            }
        }
    }

    // Extracts the value of a named HTML attribute. Handles single and double quotes.
    static String _extractAttr(const String& tag, const char* attrName) {
        String lower = tag;
        lower.toLowerCase();
        String needle = String(attrName) + "=\"";
        int pos = lower.indexOf(needle);
        char quote = '"';
        if (pos < 0) {
            needle = String(attrName) + "='";
            pos = lower.indexOf(needle);
            quote = '\'';
        }
        if (pos < 0) return "";
        int start = pos + needle.length();
        int end = tag.indexOf(quote, start);
        if (end < 0) return "";
        return tag.substring(start, end);
    }

    // Returns true if the tag typically causes a visual line break.
    // Compares only the tag name, ignoring any attributes still in the string.
    static bool _isBlockTag(const String& tag) {
        String name = tag;
        int sp = name.indexOf(' ');
        if (sp > 0) name = name.substring(0, sp);
        return name == "p" || name == "/p" ||
               name == "br" || name == "br/" ||
               name == "div" || name == "/div" ||
               name == "h1" || name == "h2" || name == "h3" ||
               name == "h4" || name == "h5" ||
               name == "/h1" || name == "/h2" || name == "/h3" ||
               name == "li" || name == "tr" || name == "/tr" ||
               name == "article" || name == "/article" ||
               name == "section" || name == "/section";
    }

    static char _decodeEntity(const String& name) {
        if (name == "amp") return '&';
        if (name == "lt") return '<';
        if (name == "gt") return '>';
        if (name == "quot") return '"';
        if (name == "apos") return '\'';
        if (name == "nbsp") return ' ';
        if (name == "copy") return 'c';
        if (name == "reg") return 'r';
        if (name == "mdash" || name == "ndash") return '-';
        if (name == "laquo" || name == "raquo") return '"';
        if (name == "hellip") return '.';
        if (name.startsWith("#x") || name.startsWith("#X")) {
            long val = strtol(name.c_str() + 2, nullptr, 16);
            if (val > 31 && val < 127) return (char)val;
        }
        if (name.startsWith("#")) {
            long val = name.substring(1).toInt();
            if (val > 31 && val < 127) return (char)val;
        }
        return 0;
    }
};
