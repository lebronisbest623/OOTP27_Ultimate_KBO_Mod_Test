static void kbo_webview_append_fallback_text_view(KboWindowTextBuffer* buffer)
{
            char text[32768] = {0};
            kbo_build_hub_window_text(text, sizeof(text));
            kbo_window_text_appendf(buffer, "<div class='card'><pre style=\"white-space:pre-wrap;margin:0;font-family:'Malgun Gothic';line-height:1.55\">");
            kbo_html_append_escaped(buffer, text);
            kbo_window_text_appendf(buffer, "</pre></div>");
}
