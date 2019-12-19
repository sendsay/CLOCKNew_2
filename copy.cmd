REM echo off
"c:\program files\7-zip\7z.exe" u -tgzip .\html\index .\html\index.html

"c:\program files\7-zip\7z.exe" u -tgzip .\html\style .\html\style.css
"c:\program files\7-zip\7z.exe" u -tgzip .\html\script .\html\script.js
rename .\html\index.gz index.html.gz
rename .\html\style.gz style.css.gz
rename .\html\script.gz script.js.gz

move /Y .\html\index.html.gz .\data
move /Y .\html\style.css.gz .\data
move /Y .\html\script.js.gz .\data

C:\Users\Professional\.platformio\penv\Scripts\platformio.exe run --target erase
C:\Users\Professional\.platformio\penv\Scripts\platformio.exe run --target uploadfs
C:\Users\Professional\.platformio\penv\Scripts\platformio.exe run --target upload --target monitor
