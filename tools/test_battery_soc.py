"""Run the real /batt parser on a PC using the project's ArduinoJson headers.

No network or ESP32 hardware is used. Only the clock, state and lock are stubbed.
"""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser()
p.add_argument('--compiler',required=True,help='Native C++ compiler, or zig executable')
p.add_argument('--arduinojson',required=True,help='ArduinoJson src directory')
p.add_argument('--project',type=Path,default=Path(__file__).resolve().parent.parent)
a=p.parse_args();root=a.project
source=(root/'http_handler.cpp').read_text(encoding='utf-8')
helpers=source[source.index('static float jsonFloat('):source.index('static bool jsonBool(')]
parser=source[source.index('static void parse_batt_json('):source.index('// GET /shore')]
with tempfile.TemporaryDirectory(prefix='camperui-soc-') as directory:
    folder=Path(directory);(folder/'battery_soc_under_test.h').write_text(helpers+'\n'+parser,encoding='utf-8')
    output=folder/('test_soc.exe' if __import__('os').name=='nt' else 'test_soc')
    command=[a.compiler]
    if Path(a.compiler).stem.lower()=='zig':command+=['c++']
    command+=['-std=c++17','-I',a.arduinojson,'-I',str(folder),str(root/'tools/test_battery_soc.cpp'),'-o',str(output)]
    subprocess.run(command,check=True)
    subprocess.run([str(output)],check=True)
