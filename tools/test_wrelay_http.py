"""Compile actual http_put packet selection, then exercise it through real Node-RED HTTP In."""
from pathlib import Path
import argparse,subprocess,tempfile,os
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True)
p.add_argument('--node-red',required=True,help='Installed node-red package directory (not a VanPi address)')
p.add_argument('--project',type=Path,default=Path(__file__).resolve().parent.parent)
a=p.parse_args();root=a.project
source=(root/'http_handler.cpp').read_text(encoding='utf-8')
function=source[source.index('static void http_put('):source.index('static RelayClickFilter relay_click_filter;')]
with tempfile.TemporaryDirectory(prefix='camperui-wrelay-transport-') as d:
    folder=Path(d);(folder/'wrelay_transport_under_test.h').write_text(function,encoding='utf-8')
    output=folder/('test_transport.exe' if os.name=='nt' else 'test_transport')
    command=[a.compiler]+(['c++'] if Path(a.compiler).stem.lower()=='zig' else [])
    subprocess.run(command+['-std=c++17','-I',str(folder),str(root/'tools/test_wrelay_transport.cpp'),'-o',str(output)],check=True)
    result=subprocess.run([str(output)],check=True,capture_output=True,text=True)
    requests=folder/'requests.jsonl';requests.write_text(result.stdout,encoding='utf-8')
    print('PASS: actual http_put selects text only for /wrelay/, preserves other packets, guards and timeouts.',flush=True)
    subprocess.run(['node',str(root/'tools/test_wrelay_http.cjs'),a.node_red,str(root),str(requests)],check=True,timeout=120)
