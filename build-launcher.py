#!/usr/bin/env python3
"""Build the launcher bundle from source and an existing Wine Staging 11.21 app."""
from pathlib import Path
import argparse
import hashlib
import runpy
import shutil
import subprocess

project = Path(__file__).resolve().parent
source = project/'Source'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wine-app',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args = parser.parse_args()
original = args.wine_app.resolve()
bridge = Path('Contents/Resources/wine/lib/wine/x86_64-windows/wow64cpu.dll')
patched_hash = 'b6d99d573fd94f714545542d39ff0bc9124c186bfd8fc552d819326eb8e93214'
original_hash = '261da16d45a5196eb64a1173387120f8b0422ac4e9c74b3a4edb234385cb5fa5'
if not (original/bridge).is_file(): parser.error('Wine app is missing its 64-bit wow64cpu.dll')
runtime_hash = hashlib.sha256((original/bridge).read_bytes()).hexdigest()
if runtime_hash not in (original_hash,patched_hash): parser.error('This is not the supported Wine Staging 11.21 build')
app = args.output.resolve()
if app.exists() or app.suffix!='.app': parser.error('Output must be a new .app path')
resources = app/'Contents/Resources'
(app/'Contents/MacOS').mkdir(parents=True)
resources.mkdir()
subprocess.run(['/usr/bin/ditto',str(original),str(resources/'Wine Staging.app')],check=True)
if runtime_hash != patched_hash:
    ns = runpy.run_path(str(source/'WineRosetta/patch_wow64cpu.py'))
    target = resources/'Wine Staging.app'/bridge
    ns['patch'](target,target)
shutil.copytree(source,resources/'Source')
shutil.copy2(source/'Info.plist',app/'Contents/Info.plist')
shutil.copy2(source/'GamePatch.json',resources/'GamePatch.json')
shutil.copy2(source/'play.sh',resources/'play.sh')
(resources/'play.sh').chmod(0o755)
for name in ('sha_v01.cfg','Arcade.icns'):
    shutil.copy2(source/'BundleResources'/name,resources/name)
(resources/'Support').mkdir()
shutil.copy2(source/'BundleResources/macd.dll',resources/'Support/macd.dll')
for name in ('LAUNCHER.md','THIRD_PARTY.md','LICENSE'):
    shutil.copy2(project/name,resources/name)
subprocess.run(['/usr/bin/xcrun','clang','-fobjc-arc','-O2','-Wall','-Wextra','-Werror',
    '-Wno-deprecated-declarations','-target','arm64-apple-macos13.0','-framework','Cocoa',
    '-framework','UniformTypeIdentifiers',str(source/'Launcher.m'),'-o',str(app/'Contents/MacOS/Play')],check=True)
subprocess.run(['/usr/bin/codesign','--force','--sign','-',str(app)],check=True)
print('Built local launcher:',app)
print('Use package-release.py --app PATH --test to create a test download.')
