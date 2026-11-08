"""Run local display-battery model and production polling logic with an I2C mock."""
from pathlib import Path
import argparse,subprocess,tempfile,os
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True)
p.add_argument('--project',type=Path,default=Path(__file__).resolve().parent.parent)
a=p.parse_args();root=a.project
source=(root/'display_battery.cpp').read_text(encoding='utf-8')
with tempfile.TemporaryDirectory(prefix='camperui-display-battery-') as d:
    folder=Path(d)
    (folder/'display_battery_under_test.h').write_text('\n'.join(line for line in source.splitlines() if not line.startswith('#include')),encoding='utf-8')
    output=folder/('display_battery_test.exe' if os.name=='nt' else 'display_battery_test')
    cmd=[a.compiler]+(['c++'] if Path(a.compiler).stem.lower()=='zig' else [])
    subprocess.run(cmd+['-std=c++17','-I',str(root),'-I',str(folder),str(root/'tools/test_display_battery.cpp'),'-o',str(output)],check=True)
    subprocess.run([str(output)],check=True)
