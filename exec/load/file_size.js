// file_size.js

// Function for returning a string representation of a file size

function file_size_str(size, bytes, precision)
{
	if(precision !== undefined)
		return file_size_float(size, /* unit: */bytes, precision);
	if(bytes) {
		if(size < 1000)        /* Bytes */
			return format("%ldB",size);
		if(size<10000)         /* Bytes with comma */
			return format("%ld,%03ldB",(size/1000),(size%1000));
	}
	if(size == 0)
		return "0K";
	size = size/1024;
	if(size<1000)  /* KB */
		return format("%ldK", Math.ceil(size));
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

// ported from xpdev/genwrap.c byte_estimate_to_str()
function file_size_float(size, unit, precision)
{
	const one_tebibyte = 1024.0*1024.0*1024.0*1024.0;
	const one_gibibyte = 1024.0*1024.0*1024.0;
	const one_mebibyte = 1024.0*1024.0;
	const one_kibibyte = 1024.0;

	if(size >= one_tebibyte)
		return format("%1.*fT", precision, size/one_tebibyte);
	if(size >= one_gibibyte || unit == one_gibibyte)
		return format("%1.*fG", precision, size/one_gibibyte);
	if(size >= one_mebibyte || unit == one_mebibyte)
		return format("%1.*fM", precision, size/one_mebibyte);
	else if(size >= one_kibibyte || unit == one_kibibyte)
		return format("%1.*fK", precision, size/one_kibibyte);
	else
		return format("%luB", size);
}
