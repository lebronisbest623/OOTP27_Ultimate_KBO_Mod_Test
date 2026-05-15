#include "../../hotkey_window_webview_internal.h"
#include "../../../content/hotkey_window_runtime_content_internal.h"
#include "../../../player_hover/player_hover_manager_probe.h"
#include "../../../../support/assets/paths/ui_image_sources.h"
#include "../../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"

static void kbo_hide_webview_player_tooltip(void);

static void kbo_append_rawf(char* out, size_t out_size, size_t* pos, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *pos, out_size - *pos, fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    size_t advance = (size_t)written;
    if (advance >= out_size - *pos) {
        *pos = out_size - 1u;
        out[*pos] = '\0';
        return;
    }

    *pos += advance;
}

static void kbo_append_js_literal(char* out, size_t out_size, size_t* pos, const char* text)
{
    if (out == NULL || out_size == 0u || pos == NULL || text == NULL) {
        return;
    }

    kbo_append_rawf(out, out_size, pos, "'");
    for (const unsigned char* p = (const unsigned char*)text; *p != '\0' && *pos + 8u < out_size; ++p) {
        unsigned char ch = *p;
        if (ch == '\\' || ch == '\'') {
            kbo_append_rawf(out, out_size, pos, "\\%c", (int)ch);
        } else if (ch == '\n') {
            kbo_append_rawf(out, out_size, pos, "\\n");
        } else if (ch == '\r') {
            kbo_append_rawf(out, out_size, pos, "\\r");
        } else if (ch == '\t') {
            kbo_append_rawf(out, out_size, pos, "\\t");
        } else if (ch < 0x20u) {
            kbo_append_rawf(out, out_size, pos, "\\x%02x", (unsigned int)ch);
        } else {
            out[*pos] = (char)ch;
            *pos += 1u;
            out[*pos] = '\0';
        }
    }
    kbo_append_rawf(out, out_size, pos, "'");
}

static int kbo_webview_execute_utf8_script(const char* script)
{
    if (script == NULL || g_kbo_webview == NULL) {
        return 0;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, script, -1, NULL, 0);
    if (wide_len <= 0) {
        return 0;
    }

    WCHAR* wide_script = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
    if (wide_script == NULL) {
        return 0;
    }

    int converted = MultiByteToWideChar(CP_UTF8, 0, script, -1, wide_script, wide_len);
    if (converted <= 0) {
        HeapFree(GetProcessHeap(), 0, wide_script);
        return 0;
    }

    HRESULT hr = ICoreWebView2_ExecuteScript(g_kbo_webview, wide_script, NULL);
    HeapFree(GetProcessHeap(), 0, wide_script);
    return SUCCEEDED(hr);
}

static int kbo_find_player_portrait_src(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || player_id == 0u) {
        return 0;
    }
    out[0] = '\0';

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char portrait_path[MAX_PATH] = {0};
    int written = snprintf(
        portrait_path,
        sizeof(portrait_path),
        "%s\\news\\html\\images\\person_pictures\\player_%u.png",
        save_path,
        player_id);
    if (written <= 0 || (size_t)written >= sizeof(portrait_path)) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(portrait_path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return 0;
    }

    kbo_webview_copy_file_url(portrait_path, out, out_size);
    return out[0] != '\0';
}

static void kbo_show_webview_player_tooltip_shell(const char* player_name, int client_x, int client_y, uint32_t hover_seq)
{
    char script[8192] = {0};
    size_t pos = 0u;
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        "(function(){if(window.__kboPlayerHoverSeq!==%u){return;}var tip=document.getElementById('kboPlayerTooltip');"
        "if(!tip){tip=document.createElement('div');tip.id='kboPlayerTooltip';document.body.appendChild(tip);}"
        "tip.setAttribute('data-kbo-hover-seq','%u');tip.textContent=",
        hover_seq,
        hover_seq);
    kbo_append_js_literal(script, sizeof(script), &pos, player_name != NULL && player_name[0] != '\0' ? player_name : "Player");
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        ";tip.style.cssText='position:fixed;z-index:2147483647;left:%dpx;top:%dpx;"
        "min-width:220px;min-height:42px;padding:10px 12px;background:#2c2d30;color:#fff;"
        "border:1px solid #f05024;box-shadow:0 8px 22px rgba(0,0,0,.75);"
        "font-family:var(--ui-font),\\'Malgun Gothic\\',sans-serif;font-size:14px;font-weight:800;pointer-events:none;display:block';})();",
        client_x + 14,
        client_y + 14);
    if (!kbo_webview_execute_utf8_script(script)) {
        kbo_log_runtime_line("webview player tooltip shell execute failed");
    }
}

static void kbo_show_webview_player_tooltip(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    const char* player_name,
    int client_x,
    int client_y,
    uint32_t hover_seq)
{
    (void)player;
    (void)team_id;
    (void)league_id;

    kbo_show_webview_player_tooltip_shell(player_name, client_x, client_y, hover_seq);

    char payload[16000] = {0};
    if (!kbo_capture_ootp_player_tooltip_payload(player_id, payload, sizeof(payload))) {
        kbo_log_runtimef("webview player tooltip skipped reason=capture_failed player=%u", player_id);
        kbo_hide_webview_player_tooltip();
        return;
    }

    char portrait_src[32768] = {0};
    kbo_find_player_portrait_src(player_id, portrait_src, sizeof(portrait_src));
    char team_primary[16] = "#f05024";
    char team_secondary[16] = "#303135";
    kbo_hub_copy_team_bar_colors(team_id, team_primary, sizeof(team_primary), team_secondary, sizeof(team_secondary));

    char simple_script[40000] = {0};
    size_t simple_pos = 0u;
    kbo_append_rawf(
        simple_script,
        sizeof(simple_script),
        &simple_pos,
        "(function(){try{var hoverSeq=%u;if(window.__kboPlayerHoverSeq!==hoverSeq){return;}var payload=",
        hover_seq);
    kbo_append_js_literal(simple_script, sizeof(simple_script), &simple_pos, payload);
    kbo_append_rawf(simple_script, sizeof(simple_script), &simple_pos, ";var playerName=");
    kbo_append_js_literal(simple_script, sizeof(simple_script), &simple_pos, player_name != NULL ? player_name : "");
    kbo_append_rawf(simple_script, sizeof(simple_script), &simple_pos, ";var portraitSrc=");
    kbo_append_js_literal(simple_script, sizeof(simple_script), &simple_pos, portrait_src);
    kbo_append_rawf(simple_script, sizeof(simple_script), &simple_pos, ";var teamPrimary=");
    kbo_append_js_literal(simple_script, sizeof(simple_script), &simple_pos, team_primary);
    kbo_append_rawf(simple_script, sizeof(simple_script), &simple_pos, ";var teamSecondary=");
    kbo_append_js_literal(simple_script, sizeof(simple_script), &simple_pos, team_secondary);
    kbo_append_rawf(
        simple_script,
        sizeof(simple_script),
        &simple_pos,
        ";var playerId=%u;function esc(s){return String(s==null?'':s).replace(/[&<>\\\"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','\\\"':'&quot;'}[c]||c;});}"
        "function section(name,next){var a=payload.indexOf(name);if(a<0){return '';}a+=name.length;var b=next?payload.indexOf(next,a):-1;return (b>=0?payload.slice(a,b):payload.slice(a)).trim();}"
        "function clean(text){var full=String(playerName||'').trim();var id=String(playerId||'');var bits={};full.split(/\\s+/).forEach(function(p){if(p){bits[p]=1;}});return text.split(/\\r?\\n/).map(function(s){return s.trim();}).filter(function(s){if(!s){return false;}if(s.indexOf('$APPDATA')>=0||s.indexOf('C:/')>=0||s.indexOf('Documents/Out of the Park')>=0||s.indexOf('person_pictures')>=0){return false;}if(s==='player_popup'||s==='background'||s==='grid_title'||s==='png'||s==='.png'||s==='rb'||s==='player_'){return false;}if(/^player_\\d+(?:\\.png)?$/i.test(s)){return false;}if(id&&s===id){return false;}if(full&&s===full){return false;}if(bits[s]){return false;}return true;});}"
        "function jerseyNumber(lines){for(var i=0;i<lines.length;i++){var m=String(lines[i]).match(/^#(\\d{1,3})$/);if(m){return +m[1];}if(lines[i]==='#'&&i+1<lines.length&&/^\\d{1,3}$/.test(lines[i+1])){return +lines[i+1];}}return 0;}"
        "function ratingRows(lines,mem){var rows=[],seen={},jersey=jerseyNumber(lines);function add(label,a,b){if(!(a>0)){return;}if(b&&!(b>0)){b=0;}if(label!=='OVR'&&label!=='POT'&&a===jersey&&!b){return;}var key=label+'|'+a+'|'+(b||'');if(seen[key]){return;}seen[key]=1;rows.push(b?[label,a,b]:[label,a]);}mem.split(/\\r?\\n/).forEach(function(s){var m=s.trim().match(/^(OVR|POT)\\s+(\\d{1,4})$/);if(m){add(m[1],+m[2],0);}});var labels={OVR:1,POT:1,STU:1,MOV:1,CON:1,STA:1,POW:1,EYE:1,DEF:1};var p='',nums=[],slash=false;function flush(){if(p&&nums.length){if(slash&&nums.length>1){add(p,nums[0],nums[1]);}else{add(p,nums[0],0);}}p='';nums=[];slash=false;}for(var i=0;i<lines.length&&lines[i]!=='Year';i++){var t=lines[i];if(labels[t]){flush();p=t;continue;}if(p&&(t.charAt(0)==='#'||t.indexOf('Age ')===0||t.indexOf('| Age ')>=0)){flush();continue;}if(p&&t==='/'){slash=true;continue;}if(p&&/^\\d+$/.test(t)){var v=+t;if(v<=0){continue;}if(p==='OVR'||p==='POT'){add(p,v,0);p='';nums=[];slash=false;}else if(!nums.length){nums.push(v);}else if(slash){nums.push(v);flush();}else{flush();}}}flush();return rows;}"
        "function statWidth(h){return {Year:47,TM:43,LG:44,G:30,AB:35,HR:30,RBI:34,AVG:45,OBP:45,SLG:45,W:31,L:31,SV:34,IP:47,BB:35,K:35,ERA:48}[h]||34;}"
        "function statTable(lines){var statHeaders={Year:1,TM:1,LG:1,G:1,AB:1,HR:1,RBI:1,AVG:1,OBP:1,SLG:1,W:1,L:1,SV:1,IP:1,BB:1,K:1,ERA:1};function isYear(s){return /^\\d{4}$/.test(s);}function isOrg(s){return /^[A-Za-z0-9]{2,6}$/.test(s)&&/[A-Za-z]/.test(s)&&!statHeaders[s];}function isNum(s){return /^\\d+$/.test(s)||/^\\.\\d+$/.test(s)||/^\\d+\\.\\d+$/.test(s);}function dedupeDecimalStats(nums,need){if(nums.length<=need){return nums;}var out=[];for(var n=0;n<nums.length;n++){var v=nums[n];out.push(v);if(String(v).indexOf('.')>=0&&n+1<nums.length&&nums[n+1]===v){n++;}}return out;}var hi=lines.indexOf('Year');if(hi<0){return '';}var headers=[];var i=hi;for(;i<lines.length;i++){if(isYear(lines[i])){break;}if(statHeaders[lines[i]]){headers.push(lines[i]);}}var rows=[];while(i<lines.length){if(!isYear(lines[i])){i++;continue;}var year=lines[i++];var seg=[];while(i<lines.length&&!isYear(lines[i])){seg.push(lines[i++]);}var org=[];var j=0;for(;j<seg.length;j++){if(!isOrg(seg[j])){break;}if(org.indexOf(seg[j])<0){org.push(seg[j]);}}var nums=[];for(;j<seg.length;j++){var t=seg[j];if(t==='.'&&nums.length&&j+1<seg.length&&/^\\d+$/.test(seg[j+1])){nums[nums.length-1]+='.'+seg[j+1];j++;continue;}if(t==='.'&&j+1<seg.length&&/^\\d+$/.test(seg[j+1])){nums.push('.'+seg[j+1]);j++;continue;}if(isNum(t)){nums.push(t);}}var need=Math.max(0,headers.length-3);nums=dedupeDecimalStats(nums,need);var r=[year,org[0]||'-',org[1]||'-'].concat(nums.slice(0,need));while(r.length<headers.length){r.push('');}if(r.length>3){rows.push(r);}}rows=rows.slice(-3);if(!headers.length||!rows.length){return '';}return '<table><colgroup>'+headers.map(function(h){return '<col style=\"width:'+statWidth(h)+'px\">';}).join('')+'</colgroup><thead><tr>'+headers.map(function(h){return '<th>'+esc(h)+'</th>';}).join('')+'</tr></thead><tbody>'+rows.map(function(r){return '<tr>'+headers.map(function(h,idx){return '<td>'+esc(r[idx]||'')+'</td>';}).join('')+'</tr>';}).join('')+'</tbody></table>';}"
        "function maxRating(rows){var m=0;for(var i=0;i<rows.length;i++){m=Math.max(m,+rows[i][1]||0,+rows[i][2]||0);}return m>100?250:100;}"
        "function norm(v){return Math.max(0,Math.min(100,((+v||0)/ratingScale)*100));}"
        "function ratingColor(v){var n=norm(v);return n>=90?'#00a8ff':n>=80?'#13b7c9':n>=50?'#2fc51a':n>=35?'#ffd22d':n>=20?'#f08a1f':'#d84a35';}"
        "function overallColor(v){var n=norm(v);return n>=70?'#00a8ff':n>=50?'#2fc51a':n>=40?'#ffd22d':n>=25?'#f08a1f':'#d84a35';}"
        "function barPct(v){return Math.max(4,Math.min(100,Math.round(norm(v))));}"
        "function valueHtml(r){var ca=ratingColor(r[1]);if(r[2]){var cb=ratingColor(r[2]);return '<b><span style=\"color:'+ca+'\">'+esc(r[1])+'</span><i> / </i><span style=\"color:'+cb+'\">'+esc(r[2])+'</span></b>';}return '<b><span style=\"color:'+ca+'\">'+esc(r[1])+'</span></b>';}"
        "function barHtml(r){var a=barPct(r[1]);var ca=ratingColor(r[1]);if(r[2]){var b=barPct(r[2]);var cb=ratingColor(r[2]);var max=Math.max(a,b);return '<div class=\"kboBar\"><em class=\"kboBarFill kboBarPotential\" style=\"width:'+max+'%%;background:'+cb+'\"></em><em class=\"kboBarFill\" style=\"width:'+a+'%%;background:'+ca+'\"></em></div>';}return '<div class=\"kboBar\"><em class=\"kboBarFill\" style=\"width:'+a+'%%;background:'+ca+'\"></em></div>';}"
        "function row(r){return '<div class=\"kboRatingRow\"><span>'+esc(r[0])+'</span>'+valueHtml(r)+barHtml(r)+'</div>';}"
        "function overallRow(ratings){var o=null,p=null,rest=[];for(var i=0;i<ratings.length;i++){if(ratings[i][0]==='OVR'&&!o){o=ratings[i];}else if(ratings[i][0]==='POT'&&!p){p=ratings[i];}else{rest.push(ratings[i]);}}var h='';if(o||p){h='<div class=\"kboOverall\"><span>OVR</span><b style=\"color:'+overallColor(o?o[1]:0)+'\">'+esc(o?o[1]:'')+'</b><span>POT</span><b style=\"color:'+overallColor(p?p[1]:0)+'\">'+esc(p?p[1]:'')+'</b></div>';}return {html:h,rest:rest};}"
        "function firstTeam(lines){var y=lines.indexOf('Year');for(var i=y+1;i<lines.length;i++){if(/^\\d{4}$/.test(lines[i])){return lines[i+1]||'';}}return '';}"
        "function titleLine(lines){var pos='';for(var i=0;i<lines.length&&lines[i]!=='OVR';i++){if(/^(SP|RP|CL|C|1B|2B|3B|SS|LF|CF|RF|DH)$/.test(lines[i])){pos=lines[i];break;}}var tm=firstTeam(lines);return (pos?pos+' ':'')+(playerName||'Player')+(tm?', '+tm:'');}"
        "var cap=section('render text append capture','memory display ratings');var mem=section('memory display ratings','');var lines=clean(cap);var allRatings=ratingRows(lines,mem);var ratingScale=maxRating(allRatings);var grouped=overallRow(allRatings);var ratings=grouped.rest.slice(0,4);var stats=statTable(lines);var title=titleLine(lines);"
        "var meta='';for(var i=0;i<lines.length;i++){if(lines[i].indexOf('| Age ')>=0||lines[i].indexOf('Age ')===0){meta=lines[i];break;}}"
        "if(window.__kboPlayerHoverSeq!==hoverSeq){return;}var tip=document.getElementById('kboPlayerTooltip');if(!tip){tip=document.createElement('div');tip.id='kboPlayerTooltip';document.body.appendChild(tip);}tip.setAttribute('data-kbo-hover-seq',String(hoverSeq));"
        "var portrait=portraitSrc?'<img class=\"kboPortrait\" src=\"'+esc(portraitSrc)+'\" onerror=\"this.style.visibility=\\'hidden\\'\">':'<div class=\"kboPortrait\"></div>';"
        "tip.innerHTML='<div class=\"kboTop\">'+portrait+'<div class=\"kboInfo\"><div class=\"kboName\">'+esc(title)+'</div>'+grouped.html+'<div class=\"kboRatings\">'+ratings.map(row).join('')+'</div></div></div><div class=\"kboMeta\">'+esc(meta)+'</div>'+stats;"
        "var barCount=tip.querySelectorAll('.kboBar').length;var fillCount=tip.querySelectorAll('.kboBarFill').length;var firstPct=ratings.length?barPct(ratings[0][1]):0;setTimeout(function(){if(window.__kboPlayerHoverSeq===hoverSeq){location.href='kbo://player-hover/debug/'+hoverSeq+'/'+playerId+'/'+ratings.length+'/'+barCount+'/'+fillCount+'/'+ratingScale+'/'+firstPct;}},0);"
        "tip.style.cssText='position:fixed;z-index:2147483647;left:%dpx;top:%dpx;width:430px;height:auto;max-width:calc(100vw - 16px);background:#303135;color:#f3f3f3;border:1px solid #151515;box-shadow:0 8px 22px rgba(0,0,0,.72);font-family:var(--ui-font),\\'Malgun Gothic\\',sans-serif;font-size:13px;pointer-events:none;text-align:left;display:block;overflow:hidden';"
        "var css=document.getElementById('kboPlayerTipStyle');if(!css){css=document.createElement('style');css.id='kboPlayerTipStyle';document.head.appendChild(css);}css.textContent='#kboPlayerTooltip *{box-sizing:border-box}#kboPlayerTooltip .kboTop{display:grid;grid-template-columns:106px 1fr;gap:8px;height:137px;padding:7px 7px 0;background:linear-gradient(#3c3d41,#333438)}#kboPlayerTooltip .kboPortrait{width:98px;height:106px;object-fit:contain;object-position:center bottom;align-self:end;background:transparent}#kboPlayerTooltip .kboInfo{min-width:0;padding-right:3px;overflow:hidden}#kboPlayerTooltip .kboName{text-align:center;color:#aeb1b8;font-weight:800;font-size:14px;height:20px;line-height:20px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip .kboOverall{height:19px;display:grid;grid-template-columns:32px 54px 32px 1fr;gap:6px;align-items:center;color:#a8abb0;font-weight:800}#kboPlayerTooltip .kboOverall b{text-align:right;font-weight:900}#kboPlayerTooltip .kboRatings{display:grid;grid-template-rows:repeat(4,19px);gap:2px;margin-top:1px;overflow:hidden}#kboPlayerTooltip .kboRatingRow{height:19px;display:grid;grid-template-columns:36px 58px minmax(132px,1fr);gap:6px;align-items:center;color:#b8bac0;font-weight:800}#kboPlayerTooltip .kboRatingRow b{text-align:right;font-weight:900;white-space:nowrap;overflow:hidden}#kboPlayerTooltip .kboRatingRow b i{font-style:normal;color:#d9d9d9;font-weight:800}#kboPlayerTooltip .kboBar{position:relative;display:block!important;width:100%%;height:8px;background:#292a2d!important;border-radius:5px;overflow:hidden;box-shadow:inset 0 1px 0 rgba(255,255,255,.05)}#kboPlayerTooltip .kboBarFill{position:absolute;left:0;top:0;display:block!important;height:100%%;min-width:4px;border-radius:5px;box-shadow:inset 0 1px 0 rgba(255,255,255,.18)}#kboPlayerTooltip .kboBarPotential{z-index:1}#kboPlayerTooltip .kboBarFill:not(.kboBarPotential){z-index:2}#kboPlayerTooltip .kboMeta{height:25px;line-height:25px;text-align:center;background:#303135;color:#fff;font-weight:800;font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip table{width:100%%;border-collapse:collapse;table-layout:fixed;font-size:13px;line-height:1}#kboPlayerTooltip th{height:22px;background:'+teamPrimary+';color:#fff;text-align:left;padding:0 3px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:clip}#kboPlayerTooltip td{height:22px;background:#303135;color:#f4f4f4;padding:0 3px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:clip}#kboPlayerTooltip tbody tr:nth-child(even) td{background:#393a3e}#kboPlayerTooltip th:nth-child(n+4),#kboPlayerTooltip td:nth-child(n+4){text-align:right}';"
        "}catch(e){var tip=document.getElementById('kboPlayerTooltip');if(tip){tip.textContent='tooltip render error: '+e.message;tip.style.background='#4b1f1f';}}})();",
        player_id,
        client_x + 14,
        client_y + 14);
    if (!kbo_webview_execute_utf8_script(simple_script)) {
        kbo_log_runtimef("webview player tooltip simple execute failed player=%u", player_id);
    }
    return;

    char script[120000] = {0};
    size_t pos = 0u;
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        "(function(){"
        "var payload=");
    kbo_append_js_literal(script, sizeof(script), &pos, payload);
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        ";var playerName=");
    kbo_append_js_literal(script, sizeof(script), &pos, player_name != NULL ? player_name : "");
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        ";var portraitSrc=");
    kbo_append_js_literal(script, sizeof(script), &pos, portrait_src);
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        "function section(text,name,next){var a=text.indexOf(name);if(a<0){return '';}a+=name.length;var b=next?text.indexOf(next,a):-1;return (b>=0?text.slice(a,b):text.slice(a)).trim();}"
        "function cleanLines(text){return text.split(/\\r?\\n/).map(function(s){return s.trim();}).filter(function(s){return s&&s.indexOf('$APPDATA')<0&&s.indexOf('C:/')<0&&s!=='grid_title'&&s!=='player_popup'&&s!=='background'&&s!=='player_'&&s!=='.png'&&!/^player_\\d+\\.png$/.test(s);});}"
        "function isNum(s){return /^-?(?:\\d+|\\.\\d+|\\d+\\.\\d+)$/.test(s);}"
        "function parseStats(lines){var headers=[],rows=[],i=0;while(i<lines.length&&lines[i]!=='Year'){i++;}if(i>=lines.length){return {headers:[],rows:[]};}for(;i<lines.length;i++){var t=lines[i];if(/^\\d{4}$/.test(t)){break;}if(/^(Year|TM|LG|G|AB|HR|RBI|AVG|OBP|SLG|W|L|SV|IP|BB|K|ERA)$/.test(t)){headers.push(t);}}while(i<lines.length){if(!/^\\d{4}$/.test(lines[i])){i++;continue;}var row=[];row.push(lines[i++]);var org=[];while(i<lines.length&&/^[A-Z0-9]{2,6}$/.test(lines[i])&&!/^(G|AB|HR|RBI|AVG|OBP|SLG|W|L|SV|IP|BB|K|ERA)$/.test(lines[i])){if(org.length===0||org[org.length-1]!==lines[i]){org.push(lines[i]);}i++;}row.push(org[0]||'-');row.push(org.length>1?org[1]:'-');while(i<lines.length&&row.length<headers.length){if(lines[i]==='.'&&row.length>0&&i+1<lines.length&&isNum(lines[i+1])){row[row.length-1]=row[row.length-1]+'.'+lines[i+1];i+=2;continue;}if(/^\\d{4}$/.test(lines[i])){break;}if(isNum(lines[i])||/^\\.\\d+$/.test(lines[i])){row.push(lines[i]);}i++;}if(row.length>3){rows.push(row);}}return {headers:headers,rows:rows};}"
        "function parseRatings(lines){var labels={OVR:1,POT:1,CON:1,POW:1,EYE:1,DEF:1,STU:1,MOV:1,CTL:1,STA:1,PBABIP:1,HRR:1};var out=[],pending='';for(var i=0;i<lines.length;i++){var t=lines[i];if(t==='Year'){break;}if(labels[t]){pending=t;continue;}if(pending&&/^\\d+$/.test(t)){var v=+t;if(v>0&&v<=250){out.push({label:pending,a:v,b:0});pending='';}}}var compact=[];for(var j=0;j<out.length;j++){if(!compact.length||compact[compact.length-1].label!==out[j].label||compact[compact.length-1].a!==out[j].a){compact.push(out[j]);}}return compact;}"
        "function parseMemoryRatings(text){var out=[];text.split(/\\r?\\n/).forEach(function(line){var m=line.trim().match(/^(OVR|POT)\\s+(\\d{1,3})$/);if(m){var v=+m[2];if(v>0&&v<=250){out.push({label:m[1],a:v,b:0});}}});return out;}"
        "function metaFrom(lines){var joined=lines.join(' ');var age=(joined.match(/Age\\s+(\\d+)/)||[])[1]||'';var throws=(joined.match(/Throws:\\s*[- ]*([A-Za-z]+)/)||[])[1]||'';var bats=(joined.match(/Bats:\\s*[- ]*([A-Za-z]+)/)||[])[1]||'';var size=(joined.match(/(\\d+'\\s*\\d+\\\"\\s*-\\s*\\d+\\s*lbs)/)||[])[1]||'';var bits=[];if(age){bits.push('Age '+age);}if(bats){bits.push('Bats: '+bats);}if(throws){bits.push('Throws: '+throws);}if(size){bits.push(size.replace(/\\s+/g,' '));}return bits.join(' | ');}"
        "function esc(s){return String(s==null?'':s).replace(/[&<>\\\"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','\\\"':'&quot;'}[c];});}"
        "function color(v){return v>=65?'#26a816':v>=50?'#efc51c':v>=35?'#e58a1f':'#c44728';}"
        "function ratingRow(label,p){var shown=p.vb&&p.b>0?p.a+' / '+p.b:String(p.a||'');var pct=Math.max(4,Math.min(100,p.a||0));return '<div class=\"kboRatingRow\"><span>'+esc(label)+'</span><b>'+esc(shown)+'</b><i><em style=\"width:'+pct+'%;background:'+color(p.a)+'\"></em></i></div>';}"
        "function ratingsHtml(panels,isPitcher){if(!panels.length){return '';}var rows=panels.slice(0,6).map(function(p){return ratingRow(p.label,p);}).join('');return '<div class=\"kboRatings\">'+rows+'</div>';}"
        "var cap=section(payload,'render text append capture','memory display ratings')||section(payload,'render text append capture','inline strings');var mem=section(payload,'memory display ratings','inline strings');var lines=cleanLines(cap);var meta=metaFrom(lines);var stats=parseStats(lines);var isPitcher=stats.headers.indexOf('IP')>=0||stats.headers.indexOf('ERA')>=0;var panels=parseMemoryRatings(mem).concat(parseRatings(lines));"
        "var tip=document.getElementById('kboPlayerTooltip');"
        "if(!tip){tip=document.createElement('div');tip.id='kboPlayerTooltip';document.body.appendChild(tip);}"
        "var statHtml='';if(stats.headers.length&&stats.rows.length){statHtml='<table><thead><tr>'+stats.headers.map(function(h){return '<th>'+esc(h)+'</th>';}).join('')+'</tr></thead><tbody>'+stats.rows.map(function(r){return '<tr>'+stats.headers.map(function(h,idx){return '<td>'+esc(r[idx]||'')+'</td>';}).join('')+'</tr>';}).join('')+'</tbody></table>';}"
        "var portrait=portraitSrc?'<img class=\"kboPortrait\" src=\"'+esc(portraitSrc)+'\" alt=\"\">':'<div class=\"kboPortrait kboPortraitMissing\"></div>';"
        "tip.innerHTML='<div class=\"kboPlayerTipTop\">'+portrait+'<div class=\"kboTopMain\"><div class=\"kboPlayerTipHead\"><div class=\"kboPlayerTipName\">'+esc(playerName||'Player')+'</div></div>'+ratingsHtml(panels,isPitcher)+'</div></div><div class=\"kboPlayerTipMeta\">'+esc(meta||'')+'</div>'+statHtml;"
        "tip.style.cssText='position:fixed;z-index:2147483647;left:0;top:0;width:430px;max-width:calc(100vw - 16px);max-height:420px;overflow:hidden;background:#2c2d30;color:#f2f2f2;border:1px solid #121212;box-shadow:0 8px 22px rgba(0,0,0,.7);font-family:Arial,Helvetica,sans-serif;font-size:13px;pointer-events:none;text-align:left;';"
        "var css=document.getElementById('kboPlayerTipStyle');if(!css){css=document.createElement('style');css.id='kboPlayerTipStyle';document.head.appendChild(css);}css.textContent='#kboPlayerTooltip .kboPlayerTipTop{display:grid;grid-template-columns:96px 1fr;gap:10px;min-height:98px;padding:7px 8px 4px;background:#3a3b3f;border-bottom:1px solid #1c1c1c}#kboPlayerTooltip .kboPortrait{width:88px;height:88px;align-self:end;object-fit:contain;object-position:center bottom;border-radius:2px;background:#2e2f33}#kboPlayerTooltip .kboPortraitMissing{box-shadow:inset 0 0 0 1px #222;background:linear-gradient(#202124,#45464a)}#kboPlayerTooltip .kboPlayerTipHead{height:20px;text-align:center}#kboPlayerTooltip .kboPlayerTipName{font-size:14px;font-weight:800;color:#cfcfd2;line-height:20px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip .kboRatings{display:grid;gap:2px;margin-top:1px}#kboPlayerTooltip .kboRatingRow{height:17px;display:grid;grid-template-columns:38px 56px 1fr;align-items:center;gap:6px;color:#aeb0b4;font-size:13px;font-weight:800}#kboPlayerTooltip .kboRatingRow b{color:#ffd12a;text-align:right;font-weight:900}#kboPlayerTooltip .kboRatingRow i{height:8px;background:#292a2d;border-radius:5px;overflow:hidden;box-shadow:inset 0 1px 0 rgba(255,255,255,.05)}#kboPlayerTooltip .kboRatingRow em{display:block;height:100%;border-radius:5px}#kboPlayerTooltip .kboPlayerTipMeta{height:24px;line-height:24px;padding:0 8px;color:#fff;background:#303136;font-size:14px;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;text-align:center}#kboPlayerTooltip table{width:100%;border-collapse:collapse;table-layout:fixed;font-family:Arial,Helvetica,sans-serif;font-size:13px}#kboPlayerTooltip th{height:22px;background:#f05024;color:#fff;font-weight:900;text-align:left;padding:0 5px;border:0;position:static;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip td{height:22px;background:#303136;color:#f4f4f4;font-weight:700;padding:0 5px;border:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#kboPlayerTooltip tbody tr:nth-child(even) td{background:#3a3b40}#kboPlayerTooltip th:nth-child(n+4),#kboPlayerTooltip td:nth-child(n+4){text-align:right}';");
    kbo_append_rawf(
        script,
        sizeof(script),
        &pos,
        "var x=%d,y=%d;"
        "var maxX=Math.max(8,window.innerWidth-tip.offsetWidth-8);"
        "var maxY=Math.max(8,window.innerHeight-tip.offsetHeight-8);"
        "tip.style.left=Math.max(8,Math.min(x,maxX))+'px';"
        "tip.style.top=Math.max(8,Math.min(y,maxY))+'px';"
        "tip.style.display='block';"
        "})();",
        client_x + 14,
        client_y + 14);

    if (!kbo_webview_execute_utf8_script(script)) {
        kbo_log_runtimef("webview player tooltip execute failed player=%u", player_id);
    }
}

static void kbo_hide_webview_player_tooltip(void)
{
    kbo_webview_execute_utf8_script(
        "(function(){var tip=document.getElementById('kboPlayerTooltip');if(tip){tip.style.display='none';}})();");
}

static int kbo_parse_u32_segment(const char** cursor, uint32_t* out)
{
    if (cursor == NULL || *cursor == NULL || out == NULL) {
        return 0;
    }

    char* end = NULL;
    unsigned long value = strtoul(*cursor, &end, 10);
    if (end == *cursor || value > 0xfffffffful) {
        return 0;
    }

    *out = (uint32_t)value;
    *cursor = end;
    if (**cursor == '/') {
        *cursor += 1;
    }
    return 1;
}

static void kbo_request_player_hover_payload(HWND hwnd, uint32_t player_id, int client_x, int client_y, uint32_t hover_seq)
{
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &team_id, &league_id);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        kbo_log_runtimef(
            "webview player hover ignored reason=player_not_found player=%u x=%d y=%d",
            player_id,
            client_x,
            client_y);
        return;
    }

    char player_name[128] = {0};
    kbo_copy_player_display_name(player, player_name, sizeof(player_name));

    kbo_log_runtimef(
        "webview player hover payload player=%u seq=%u name=\"%s\" team=%u league=%u hwnd=%p x=%d y=%d manager=%p",
        player_id,
        hover_seq,
        player_name,
        team_id,
        league_id,
        hwnd,
        client_x,
        client_y,
        (void*)kbo_player_hover_manager_ptr());
    kbo_show_webview_player_tooltip(player, player_id, team_id, league_id, player_name, client_x, client_y, hover_seq);
}

static void kbo_hide_player_hover_payload(HWND hwnd, uint32_t player_id)
{
    (void)hwnd;
    kbo_hide_webview_player_tooltip();
    kbo_log_runtimef("webview player hover hide player=%u", player_id);
}

int kbo_webview_handle_player_hover_command(const char* cmd, HWND hwnd)
{
    if (cmd == NULL) {
        return 0;
    }

    if (strncmp(cmd, "player-hover/debug/", 19) == 0) {
        const char* cursor = cmd + 19;
        uint32_t hover_seq = 0u;
        uint32_t player_id = 0u;
        uint32_t ratings = 0u;
        uint32_t bars = 0u;
        uint32_t fills = 0u;
        uint32_t scale = 0u;
        uint32_t first_pct = 0u;
        (void)hwnd;
        if (!kbo_parse_u32_segment(&cursor, &hover_seq)
                || !kbo_parse_u32_segment(&cursor, &player_id)) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &ratings);
        (void)kbo_parse_u32_segment(&cursor, &bars);
        (void)kbo_parse_u32_segment(&cursor, &fills);
        (void)kbo_parse_u32_segment(&cursor, &scale);
        (void)kbo_parse_u32_segment(&cursor, &first_pct);
        kbo_log_runtimef(
            "webview player tooltip render debug seq=%u player=%u ratings=%u bars=%u fills=%u scale=%u first_pct=%u",
            hover_seq,
            player_id,
            ratings,
            bars,
            fills,
            scale,
            first_pct);
        return 1;
    }

    if (strncmp(cmd, "player-hover/show/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        uint32_t client_x = 0u;
        uint32_t client_y = 0u;
        uint32_t hover_seq = 0u;
        if (!kbo_parse_u32_segment(&cursor, &player_id)
                || !kbo_parse_u32_segment(&cursor, &client_x)
                || !kbo_parse_u32_segment(&cursor, &client_y)
                || player_id == 0u) {
            return 1;
        }
        (void)kbo_parse_u32_segment(&cursor, &hover_seq);

        kbo_request_player_hover_payload(hwnd, player_id, (int)client_x, (int)client_y, hover_seq);
        return 1;
    }

    if (strncmp(cmd, "player-hover/hide/", 18) == 0) {
        const char* cursor = cmd + 18;
        uint32_t player_id = 0u;
        if (kbo_parse_u32_segment(&cursor, &player_id) && player_id != 0u) {
            kbo_hide_player_hover_payload(hwnd, player_id);
        }
        return 1;
    }

    return 0;
}
