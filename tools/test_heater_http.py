"""Exercise production /heater parsing and real bundled VanPi Webasto flow on a PC."""
from pathlib import Path
import argparse,subprocess,tempfile,os
p=argparse.ArgumentParser()
p.add_argument('--compiler',required=True)
p.add_argument('--arduinojson',required=True)
p.add_argument('--project',type=Path,default=Path(__file__).resolve().parent.parent)
a=p.parse_args();root=a.project
s=(root/'http_handler.cpp').read_text(encoding='utf-8')
helpers=s[s.index('static float jsonFloat('):s.index('static bool jsonBool(')]
telemetry=s[s.index('static void parse_heater_telemetry('):s.index('static void parse_heater_json(')]
parser=s[s.index('static void parse_heater_json('):s.index('    if (state.heater_status != raw_hs)',s.index('static void parse_heater_json('))]+'}\n'
with tempfile.TemporaryDirectory(prefix='camperui-heater-') as d:
    folder=Path(d);fixture=folder/'flow-fixtures.jsonl'
    subprocess.run(['node',str(root/'tools/test_heater_diagnostics_flow.cjs'),'--fixtures',str(fixture)],check=True)
    (folder/'heater_under_test.h').write_text(helpers+telemetry+parser,encoding='utf-8')
    ui=(root/'ui_climate.cpp').read_text(encoding='utf-8')
    access=ui[ui.index('static void update_diagnostics_access()'):ui.index('static void update_ui_from_mode()')]
    (folder/'heater_ui_under_test.h').write_text(access,encoding='utf-8')
    out=folder/('test_heater.exe' if os.name=='nt' else 'test_heater')
    cmd=[a.compiler]+(['c++'] if Path(a.compiler).stem.lower()=='zig' else [])
    subprocess.run(cmd+['-std=c++17','-Wno-deprecated-declarations','-I',a.arduinojson,'-I',str(root),'-I',str(folder),str(root/'tools/test_heater_http.cpp'),'-o',str(out)],check=True)
    subprocess.run([str(out),str(fixture)],check=True)
