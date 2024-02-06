if (file_getname("/home/cyan/thefilename").indexOf('/') !== -1)
	throw new Error('slashes in filename');
