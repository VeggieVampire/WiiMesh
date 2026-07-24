# WiiMesh FTPii Test Loop

1. Start FTPii on the Wii and wait until it shows an IP address.
2. Run:

```bat
deploy_ftpii.bat 192.168.0.13 sd
```

3. The script first downloads any existing WiiMesh data from:

```text
sd:/apps/wii-mesh/debug.log
sd:/apps/wii-mesh/messages.dat
```

4. Previous logs are saved under:

```text
outputs/logs_before_upload/
```

5. The script then rebuilds WiiMesh, copies the new `boot.dol`, and uploads:

```text
sd:/apps/wii-mesh/boot.dol
sd:/apps/wii-mesh/meta.xml
```

6. Launch WiiMesh from the Homebrew Channel.
7. For each new build, check `work/WiiMesh/CHANGELOG.md` to see what changed.
