"""Kjør tester uten tilkoblet maskinvare. Krever Python og g++ på PATH."""
from pathlib import Path
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parent.parent
compiler=shutil.which('g++')
if not compiler:
    raise SystemExit('g++ mangler på PATH; alternativt bruk CMake-testmålene i CLion.')
build=root/'build'
build.mkdir(exist_ok=True)
subprocess.run([sys.executable,'-m','unittest','discover','-s','tests','-v'],cwd=root,check=True)
common=[str(root/'src/control'/name) for name in ('linefollow.cpp','helper.cpp','ramps.cpp')]
for source,name,fakes in [('test_runtime.cpp','runtime_tests',False),('test_nano.cpp','nano_runtime_tests',True)]:
    output=build/(name+'.exe' if sys.platform=='win32' else name)
    includes=['-I',str(root/'include'),'-I',str(root/'include/control')]
    if fakes:
        includes=['-I',str(root/'tests/fakes')]+includes
    subprocess.run([compiler,'-std=c++17','-Wall','-Wextra',*includes,str(root/'tests'/source),*common,'-o',str(output)],check=True)
    subprocess.run([str(output)],check=True)
