For macOS App Bundle packaging: Place the built 'scratch-pc' executable in this folder, next to this 'README.md' file before trying to do anything else. If you do not want to waste money on Apple's Mac App Developer Program, running these commands in the Terminal App will allow the app to run on any target machine free of charge (note the end user will need to do this on their machine after downloading SE!):
```sh
# Replace line below with correct path in the quotes:
cd "/path/to/scratch/everywhere/app/parent/directory";
chmod u+x "./scratch-pc.app/Contents/MacOS/scratch-pc";
xattr -dr com.apple.quarantine "./scratch-pc.app";
```
When distributing, this 'README.md' file is not necessary, as it is only for information purposes, and it can be deleted if you do not want it.
