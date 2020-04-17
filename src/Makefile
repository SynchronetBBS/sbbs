all: tags cscope.out

cscope.out: cscope.files
	cscope -b

tags: cscope.files
	exctags -VL cscope.files

cscope.files::
	find build comio conio encode hash sbbs3 sexpots smblib syncdraw syncterm tone uifc xpdev -name '*.c' -o -name '*.cpp' -o -name '*.h' > cscope.files
