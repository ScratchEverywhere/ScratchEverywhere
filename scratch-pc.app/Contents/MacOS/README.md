For macOS App Bundle packaging: Place the built 'scratch-pc' executable in this folder, next to this 'README.md' file before doing anything else. If you do not want to waste money on Apple's Mac App Developer Program, running this command in the Terminal App will allow the app to run:
```sh
chmod u+x /path/to/scratch-pc.app/Contents/MacOS/scratch-pc && xattr -d -r com.apple.quarantine /path/to/scratch-pc.app
```
