"""Kjør tester uten tilkoblet maskinvare. Krever Python og g++ på PATH."""
from pathlib import Path
import shutil
import subprocess
import sys
import json

root=Path(__file__).resolve().parent.parent
compiler=shutil.which('g++')
if not compiler:
    raise SystemExit('g++ mangler på PATH; alternativt bruk CMake-testmålene i CLion.')
build=root/'build'
build.mkdir(exist_ok=True)
subprocess.run([sys.executable,'-m','unittest','discover','-s','tests','-v'],cwd=root,check=True)
common=[str(root/'src/control'/name) for name in ('linefollow.cpp','helper.cpp','ramps.cpp')]
for source,name,fakes in [('test_runtime.cpp','runtime_tests',False),('test_nano.cpp','nano_runtime_tests',True),
                          ('test_master.cpp','master_tests',True),('test_side_link.cpp','side_link_tests',True),
                          ('test_side_nano.cpp','side_nano_tests',True)]:
    output=build/(name+'.exe' if sys.platform=='win32' else name)
    includes=['-I',str(root/'include'),'-I',str(root/'include/control')]
    if fakes:
        includes=['-I',str(root/'tests/fakes')]+includes
    sources=common
    if name=='side_nano_tests':
        includes=['-I',str(root/'tests/side_config')]+includes
        sources=[]
    subprocess.run([compiler,'-std=c++17','-Wall','-Wextra',*includes,str(root/'tests'/source),*sources,'-o',str(output)],check=True)
    result=subprocess.run([str(output)],check=True,capture_output=True,text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if not line.startswith('{')))
    if name=='master_tests':
        messages=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        samples=[m for m in messages if m['kind']=='sample']
        assert len(samples)==2 and samples[0]['time_us']==0xF1234567
        assert samples[0]['side_raw']==[None]*6
        assert samples[1]['side_raw'][:2]==[0,950]
        assert len(samples[1]['main_indices'])==6 and len(samples[1]['side_indices'])==6
