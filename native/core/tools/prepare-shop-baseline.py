import io, pathlib, subprocess, tarfile, shutil
root=pathlib.Path('native/core/build-shops/baseline-source')
root.mkdir(parents=True,exist_ok=True)
archive=subprocess.check_output(['git','archive','--format=tar','HEAD','native'])
with tarfile.open(fileobj=io.BytesIO(archive)) as tf:
    tf.extractall(root,filter='data')
shutil.copyfile('native/core/src/loot_names.inc',root/'native/core/src/loot_names.inc')
