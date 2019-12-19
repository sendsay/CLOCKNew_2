"c:\program files\7-zip\7z.exe" u -tgzip index index.html
"c:\program files\7-zip\7z.exe" u -tgzip style style.css
"c:\program files\7-zip\7z.exe" u -tgzip script script.js
rename index.gz index.html.gz
rename style.gz style.css.gz
rename script.gz script.js.gz

move /Y index.html.gz ..\data
move /Y style.css.gz ..\data
move /Y script.js.gz ..\data
REM pause