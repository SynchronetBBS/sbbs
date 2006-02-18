all: tags cscope.out

cscope.out: cscope.files
	cscope -b

tags: cscope.files
	exctags -VL cscope.files

cscope.files::
	find build conio sbbs3 smblib uifc xpdev -name '*.c' -o -name '*.cpp' -o -name '*.h' > cscope.files
