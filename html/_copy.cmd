"c:\program files\7-zip\7z.exe" u -tgzip index index.html
"c:\program files\7-zip\7z.exe" u -tgzip style style.css
rename index.gz index.html.gz
rename style.gz style.css.gz
move /Y index.html.gz ..\data
move /Y style.css.gz ..\data
rem pause