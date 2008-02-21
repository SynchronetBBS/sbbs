function fname(name)
{
	if(name.search(/^(?:[A-Z]:)?[\/\\]/)!=-1)
		return(name);
	return(startup_path+name);
}
