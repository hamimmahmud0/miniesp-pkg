import http.server, json, os, sys, urllib.parse, time
ROOT=sys.argv[1]; PORT=int(sys.argv[2])
CFG={"name":"tank1","pump":"relay_node_1/relay/waterpump","ha":"homeassistant","trig":33,"echo":32,"temp":0,"empty_mm":1379,"full_mm":205,"vol_l":1000,"ml_cm":8517,"on_pct":25,"off_pct":95,"max_run":30,"rest":5,"flow_min":3,"rise_pm":10,"auto":1,"hist":300,"interval":15,"samples":7,"jump_pct":15,"good_n":3,"hist_kb":16,"temp_c":20,"fault_now":"none"}
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(s,*a,**k): super().__init__(*a,directory=ROOT,**k)
    def log_message(s,*a): pass
    def do_GET(s):
        u=urllib.parse.urlparse(s.path)
        if u.path.startswith('/cgi-bin/wtms'):
            q=urllib.parse.unquote(u.query).replace('+',' ').split()
            if len(q)<2 or q[0]!='web': out={"ok":False,"error":"unknown"}
            else:
                sub=q[1]
                if sub in('me','login'): out={"ok":True,"token":"abcdefabcdefabcdefabcdef"}
                elif sub=='get': out=CFG
                elif sub in('act','auto'):
                    st=json.load(open(os.path.join(ROOT,'tank/state.json'))); out=st
                elif sub=='readj': out={"ok":True,"distance_mm":560,"level":60.0,"litres":600}
                elif sub=='mqttget': out={"ok":True,"host":"192.168.0.50","port":1883,"user":"x","client":"c","state":2}
                elif sub in('stage','commit','set'): out={"ok":True}
                else: out={"ok":True}
            b=json.dumps(out).encode(); s.send_response(200); s.send_header('Content-Type','application/json'); s.send_header('Content-Length',str(len(b))); s.end_headers(); s.wfile.write(b); return
        return super().do_GET()
    def end_headers(s): s.send_header('Cache-Control','no-store'); super().end_headers()
http.server.ThreadingHTTPServer(('127.0.0.1',PORT),H).serve_forever()
