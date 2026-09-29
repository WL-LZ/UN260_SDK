from test_i18n_support import with_i18n
from pathlib import Path
import tempfile,subprocess,os,shutil,ctypes.util
from PIL import Image
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-workspace-') as directory:
    temp=Path(directory);data=temp/'state';usb=temp/'usb';usb.mkdir()
    Image.new('RGBA',(120,80),(35,100,210,255)).save(usb/'avatar.png')
    (usb/'un260_avatar_02.png').symlink_to(usb/'avatar.png')
    (usb/'un260_avatar_03.png').write_bytes((usb/'avatar.png').read_bytes()[:40])
    Image.new('RGB',(4096,16)).save(usb/'un260_avatar_04.png')
    binary=temp/'test'
    headers=Path(os.environ.get('UN260_SYSROOT_HEADERS') or root.parents[2]/'output/d211_d213_devkitf/host/riscv64-linux-gnu/sysroot/usr/include')
    for name in ('png.h','pngconf.h','pnglibconf.h'):shutil.copyfile(headers/name,temp/name)
    sources=[root/p for p in ['tools/test_workspace.c','un260/workspace/workspace_model.c','un260/storage/workspace_store.c']]
    sources=with_i18n(sources,root)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',f'-I{root}',f'-I{temp}',f'-DWORKSPACE_DIRECTORY="{data}"',f'-DWORKSPACE_USB_DIRECTORY="{usb}"',*map(str,sources),'-l:'+ctypes.util.find_library('png16'),'-lpthread','-Wl,--wrap=fsync','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    reports=list(usb.glob('UN260_support_*.txt'));assert len(reports)==1
    assert reports[0].read_text()=='UN260 SUPPORT\nNo personal data\n'
    file=data/'workspace.v1';original=file.read_bytes()
    temporary=data/'workspace.v1.tmp';temporary.symlink_to(usb)
    subprocess.run([str(binary),'save-failure'],check=True);assert file.read_bytes()==original
    temporary.unlink()
    subprocess.run([str(binary),'directory-sync'],check=True)
    subprocess.run([str(binary),'reload-uncertain'],check=True)
    file.write_bytes(original[:-1]+bytes([original[-1]^1]))
    corrupted=file.read_bytes();subprocess.run([str(binary),'corrupt'],check=True);assert file.read_bytes()==corrupted
    import struct,zlib
    # Explicit v1 ABI fixture from the v2 saved model; 36-byte profiles preceded
    # the identical 16388-byte avatar. No production struct cast is assumed.
    payload=original[16:]
    v2=bytearray(struct.pack('<4I',2,3,1,2))
    for i in range(8):v2+=payload[16+i*16804:16+i*16804+16752]
    v2[16+42]=1  # Existing automatic QR preference must survive migration.
    file.write_bytes(struct.pack('<4I',0x31535755,2,len(v2),zlib.crc32(v2))+v2)
    subprocess.run([str(binary),'v2'],check=True)
    old=bytearray(struct.pack('<4I',1,3,1,2))
    payload=original[16:]
    for i in range(8):
        user=payload[16+i*16804:16+i*16804+16752]
        # v2 user: 44-byte header, 8 * 40-byte profile, 16388-byte avatar
        assert len(user)==16752
        old+=user[:42]+b'\x00\x00'
        for j in range(8):old+=user[44+j*40:44+j*40+36]
        old+=user[44+8*40:]
    file.write_bytes(struct.pack('<4I',0x31535755,1,len(old),zlib.crc32(old))+old)
    subprocess.run([str(binary),'legacy'],check=True)
    assert struct.unpack('<4I',file.read_bytes()[:16])[1]==3
