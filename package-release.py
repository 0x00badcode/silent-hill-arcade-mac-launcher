#!/usr/bin/env python3
"""Sign native components inside out, package a DMG, optionally notarize it."""
from pathlib import Path
import argparse
import hashlib
import json
import plistlib
import struct
import subprocess
import tempfile
import sys

project = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--app', type=Path, default=project/'Silent Hill The Arcade.app')
parser.add_argument('--output', type=Path, default=project/'dist')
parser.add_argument('--test', action='store_true', help='Create an explicitly unnotarized test download')
parser.add_argument('--identity', help='Developer ID Application signing identity')
parser.add_argument('--notary-profile', help='Existing notarytool Keychain profile')
args = parser.parse_args()
if not args.test and (not args.identity or not args.identity.startswith('Developer ID Application:') or not args.notary_profile):
    parser.error('A public release requires --identity "Developer ID Application: …" and --notary-profile NAME. Use --test for a local test DMG.')
if args.test and (args.identity or args.notary_profile):
    parser.error('--test uses ad hoc signing; omit identity and notarization arguments.')
app = args.app.resolve()
if not app.is_dir() or app.suffix != '.app':
    parser.error('App bundle not found')
resources = app/'Contents/Resources'
for path in app.rglob('*'):
    if path.name.lower() in {'kshg.exe', 'shaiolib.crk.dll', 'shaiolib.dll'}:
        parser.error(f'Game files are present in the launcher bundle: {path}')
info = plistlib.loads((app/'Contents/Info.plist').read_bytes())
version = info['CFBundleShortVersionString']
args.output.mkdir(parents=True, exist_ok=True)
output = args.output.resolve()
name = f'Silent-Hill-Arcade-Launcher-{version}-macOS-arm64' + ('-UNNOTARIZED' if args.test else '')
dmg = output/(name+'.dmg')
if dmg.exists():
    parser.error(f'Refusing to overwrite {dmg}')

def run(*command, **kwargs):
    try:
        return subprocess.run([str(part) for part in command], check=True, **kwargs)
    except subprocess.CalledProcessError as problem:
        if problem.stderr:
            print(problem.stderr.decode(errors='replace') if isinstance(problem.stderr,bytes) else problem.stderr,file=sys.stderr)
        raise

def macho(path):
    with path.open('rb') as stream:
        head = stream.read(8)
    if head[:4] in (b'\xce\xfa\xed\xfe',b'\xcf\xfa\xed\xfe',b'\xfe\xed\xfa\xce',b'\xfe\xed\xfa\xcf'):
        return True
    if head[:4] in (b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf') and len(head)==8:
        return 0 < struct.unpack('>I',head[4:])[0] < 20
    return False

def executable(path):
    with path.open('rb') as stream:
        head = stream.read(28)
        if head[:4] in (b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'):
            fat64 = head[:4]==b'\xca\xfe\xba\xbf'
            offset = struct.unpack('>Q' if fat64 else '>I',head[16:24] if fat64 else head[16:20])[0]
            stream.seek(offset)
            head = stream.read(16)
        order = '<' if head[:4] in (b'\xce\xfa\xed\xfe',b'\xcf\xfa\xed\xfe') else '>'
        return struct.unpack(order+'I',head[12:16])[0]==2

identity = '-' if args.test else args.identity
timestamp = '--timestamp=none' if args.test else '--timestamp'
sign_base = ['/usr/bin/codesign','--force','--sign',identity,'--options','runtime',timestamp]
with tempfile.TemporaryDirectory(prefix='arcade-release-') as scratch:
    scratch = Path(scratch)
    entitlements = scratch/'wine-entitlements.plist'
    entitlements.write_bytes(plistlib.dumps({
        'com.apple.security.cs.allow-unsigned-executable-memory': True,
        'com.apple.security.cs.disable-library-validation': True,
    }))
    native = [p for p in app.rglob('*') if p.is_file() and not p.is_symlink() and macho(p)]
    engine = resources/'Wine Staging.app/Contents/Resources/wine'
    wine_loaders = {engine/'bin/wine',engine/'lib/wine/x86_64-unix/wine'}
    scanner_entitlements = scratch/'scanner-entitlements.plist'
    scanner_entitlements.write_bytes(plistlib.dumps({'com.apple.security.cs.disable-library-validation':True}))
    for path in sorted(native,key=lambda p:len(p.parts),reverse=True):
        options = ['--entitlements',entitlements] if path in wine_loaders else []
        if path not in wine_loaders and path.is_relative_to(engine) and executable(path):
            options=['--entitlements',scanner_entitlements]
        run(*sign_base,*options,path,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    bundles = [p for p in app.rglob('*') if p.is_dir() and not p.is_symlink()
        and p.suffix in {'.app','.framework','.xpc','.bundle'}
        and any((p/rel).is_file() for rel in ('Contents/Info.plist','Resources/Info.plist','Info.plist'))]
    for path in sorted(bundles,key=lambda p:len(p.parts),reverse=True):
        run(*sign_base,path,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    run(*sign_base,app)
    run('/usr/bin/codesign','--verify','--deep','--strict',app)
    print(f'Verified {len(native)} native code files and {len(bundles)+1} bundles.',flush=True)
    stage = scratch/'Disk Image'
    stage.mkdir()
    run('/usr/bin/ditto',app,stage/app.name)
    (stage/'Applications').symlink_to('/Applications')
    for doc in ('README.md','NOTICE','LICENSE'):
        run('/usr/bin/ditto',project/doc,stage/doc)
    run('/usr/bin/hdiutil','create','-volname','Silent Hill Arcade Launcher','-fs','HFS+',
        '-format','UDZO','-srcfolder',stage,dmg)
run('/usr/bin/hdiutil','verify',dmg)
if not args.test:
    run('/usr/bin/codesign','--sign',identity,'--timestamp','--identifier',info['CFBundleIdentifier']+'.dmg',dmg)
    result = run('/usr/bin/xcrun','notarytool','submit',dmg,'--keychain-profile',args.notary_profile,
        '--wait','--output-format','json',capture_output=True,text=True)
    report = json.loads(result.stdout)
    (output/(name+'-notarization.json')).write_text(json.dumps(report,indent=2)+'\n')
    run('/usr/bin/xcrun','notarytool','log',report['id'],'--keychain-profile',args.notary_profile,
        output/(name+'-notary-log.json'))
    if report.get('status') != 'Accepted':
        raise SystemExit('Notarization was not accepted. Inspect the saved notary log; this DMG is not ready to publish.')
    run('/usr/bin/xcrun','stapler','staple',dmg)
    run('/usr/bin/xcrun','stapler','validate',dmg)
    run('/usr/sbin/spctl','--assess','--type','open','--context','context:primary-signature','--verbose',dmg)
if dmg.stat().st_size >= 2*1024**3:
    raise SystemExit('DMG exceeds the GitHub release-asset size limit.')
hasher = hashlib.sha256()
with dmg.open('rb') as stream:
    for chunk in iter(lambda:stream.read(1024*1024),b''): hasher.update(chunk)
checksum = hasher.hexdigest()
(output/(name+'.sha256')).write_text(f'{checksum}  {dmg.name}\n')
print(('UNNOTARIZED TEST DOWNLOAD: ' if args.test else 'Notarized release download: ')+str(dmg))
