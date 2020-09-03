// file_size.js

// $Id: file_size.js,v 1.2 2005/03/08 03:07:17 rswindell Exp $

// Function for returning a string representation of a file size

function file_size_str(size, bytes)
{
	if(bytes) {
		if(size < 1000)        /* Bytes */
			return format("%ldB",size);
		if(size<10000)         /* Bytes with comma */
			return format("%ld,%03ldB",(size/1000),(size%1000));
	}
	size = size/1024;
	if(size<1000)  /* KB */
		return format("%ldK",size);
	if(size<100000)  /* KB With comma */
		return format("%ld,%03ldK",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* MB */
		return format("%ldM",size);
	if(size<100000) /* MB With comma */
		return format("%ld,%03ldM",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* GB */
		return format("%ldG",size);
	if(size<100000)  /* GB With comma */
		return format("%ld,%03ldG",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* TB (Yeah, right) */
		return format("%ldT",size);
	if(size<100000) /* TB With comma (Whee!) */
		return format("%ld,%03ldT",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* PB (Snicker) */
		return format("%ldP",size);
	if(size<100000) /* PB With comma (Cough!) */
		return format("%ld,%03ldP",(size/1000),(size%1000));
	/* Heck, let's go all the way! */
	size = size/1024;
	if(size<1000)   /* EB */
		return format("%ldE",size);
	if(size<100000) /* EB With comma */
		return format("%ld,%03ldE",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* ZB */
		return format("%ldZ",size);
	if(size<100000) /* ZB With comma */
		return format("%ld,%03ldZ",(size/1000),(size%1000));
	size = size/1024;
	if(size<1000)   /* YB */
		return format("%ldY",size);
	if(size<1000000) /* YB With comma */
		return format("%ld,%03ldY",(size/1000),(size%1000));

	return "Too damn big to download.";
}
