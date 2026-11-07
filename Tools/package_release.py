"""Run upstream Open MPC release.py, then add accurate docs and normalize ZIP paths.
Does not upload, install, or include sound content.
"""
from pathlib import Path
import argparse,subprocess,sys,zipfile,json,hashlib,stat
p=argparse.ArgumentParser();p.add_argument('--openmpc-tools',required=True,type=Path);p.add_argument('--module',required=True,type=Path);p.add_argument('--bench',type=Path);p.add_argument('--out',type=Path,default=Path('dist'));a=p.parse_args()
r=Path(__file__).resolve().parents[1];tools=a.openmpc_tools.resolve();out=a.out.resolve();module=a.module.resolve();skin=r/'skins/SGK - VST - SGK Hyperion'
cmd=[sys.executable,str(tools/'release.py'),'--so',str(module),'--skin',str(skin),'--entry',str(skin/'plugin-meta.xml'),'--version','0.1.2','--id','sgk-hyperion','--repo','SeanGoKrazy/SGK-Hyperion','--license','MIT','--about','Independent multi-engine player with native gold browser and editable preset FX. No sound content included.','--requires','User-supplied compatible ordinary unwrapped Hypersonic-format library; ARMv7 MPC OS 3.x with root SSH; tested on Key 37/Hakai only.','--user-data','User','--user-data','Library','-o',str(out)]
if a.bench:cmd.extend(['--bench',str(a.bench.resolve())])
subprocess.run(cmd,check=True)
z=out/'SGK-Hyperion-0.1.2-mpc-armv7.zip';top='SGK-Hyperion-0.1.2';files={};modes={}
with zipfile.ZipFile(z) as archive:
    for info in archive.infolist():
        name=info.filename.replace('\\','/');assert name.startswith(top+'/') and '..'not in name.split('/');files[name]=archive.read(info);modes[name]=info.external_attr
# Upstream currently writes OS-native paths into MODES/SHA256SUMS on Windows.
mode=top+'/MODES'
if mode in files:files[mode]=files[mode].replace(b'\\',b'/')
install=top+'/INSTALL.md';text=files[install].decode();text=text.replace('Tested on MPC OS with a Force.','Isolated tests on MPC Key 37 / user-reported Hakai MPC OS 3.9.1. Force and other hardware unverified.')
text+='\n## Library setup and prerelease status\n\nNo sounds are included. Follow LIBRARY-SETUP.md before expecting audio. This candidate adds configurable library paths without changing the DSP/GUI of the private 0.1.2 engine. Standard installer/uninstaller hardware smoke verification is still pending; publish as a GitHub prerelease only after that check. Native-coordinate preview artwork is not a captured MPC screenshot.\n'
files[install]=text.encode()
for name,src in [('LIBRARY-SETUP.md',r/'docs/LIBRARY-SETUP.md'),('VALIDATION.md',r/'docs/VALIDATION.md'),('LICENSE',r/'LICENSE'),('THIRD-PARTY-NOTICES.md',r/'THIRD-PARTY-NOTICES.md')]:files[top+'/'+name]=src.read_bytes()
key=top+'/SHA256SUMS';files[key]=''.join(hashlib.sha256(data).hexdigest()+'  '+name[len(top)+1:]+'\n' for name,data in sorted(files.items()) if name!=key).encode()
temp=z.with_suffix('.zip.new')
with zipfile.ZipFile(temp,'w',zipfile.ZIP_DEFLATED) as archive:
    for name,data in sorted(files.items()):
        info=zipfile.ZipInfo(name,(2026,11,7,0,0,0));info.create_system=3;info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=modes.get(name,(stat.S_IFREG|0o644)<<16);archive.writestr(info,data)
temp.replace(z)
(z.parent/(z.name+'.sha256')).write_text(hashlib.sha256(z.read_bytes()).hexdigest()+'  '+z.name+'\n')
subprocess.run([sys.executable,str(tools/'catalog_check.py'),str(z),'--catalog','--expect-id','sgk-hyperion','--expect-repo','SeanGoKrazy/SGK-Hyperion','--require-portable'],check=True)
print('Prepared candidate:',z)
