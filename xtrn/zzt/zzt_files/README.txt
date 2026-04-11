Drop external world packs here.

Supported layouts:

1) Single-file world at top level:
   zzt_files/MYWORLD.ZZT

2) Pack subdirectory (recommended so companion files stay together):
   zzt_files/MyPack/MYWORLD.ZZT
   zzt_files/MyPack/MYWORLD.TXT
   zzt_files/MyPack/MYWORLD.HI

In-game world picker (`W`) scans:
- zzt_files/*.ZZT
- zzt_files/*/*.ZZT
- ../zzt_files/*.ZZT
- ../zzt_files/*/*.ZZT

You can keep `.TXT` and `.HI` with the world file; only `.ZZT` is loaded by the engine.
