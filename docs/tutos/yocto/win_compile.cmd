set FILENAME=Tutoriel_YOCTO
set BIBNAME=filename
	
@echo off
cls
:question
echo 1. Compile the document
echo 2. Clean everything but keep the PDF
echo 3. Really clean EVERYTHING
set /p choix=What do you want to do (1/2/3)? :
 
if /I "%choix%"=="1" (goto :compile)
if /I "%choix%"=="2" (goto :clean)
if /I "%choix%"=="3" (goto :clean_all)
goto question
 
:compile
pdflatex %FILENAME%.tex
pdflatex %FILENAME%.tex
goto end
 
:clean
del %FILENAME%.nav
del %FILENAME%.nlo
del %FILENAME%.out
del %FILENAME%.snm
del %FILENAME%.toc
del %FILENAME%.aux
del %FILENAME%.log
goto end
 
:clean_all
del %FILENAME%.nav
del %FILENAME%.nlo
del %FILENAME%.out
del %FILENAME%.snm
del %FILENAME%.toc
del %FILENAME%.aux
del %FILENAME%.log
del %FILENAME%.pdf
goto end

:end
echo Thanks for using this tool :)