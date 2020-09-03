/*****************************************************************************/
/* SSJS templating library                                                   */
/* %%name%% is replaced with the HTML encoded value of template.name         */
/* ^^name^^ is replaced with the URI encoded value of template.name          */
/* @@name@@ is replaced with the value if template.name                      */
/*                                                                           */
/* @@name:sname@@ is replaced with the value of template.name.sname          */
/* (^^ and %% are also supported)                                            */
/*                                                                           */
/* @@JS:js_expression@@ is replaced with the return value of js_expression   */
/* (^^ and %% are also supported)                                            */
/*                                                                           */
/* <<REPEAT name>>                                                           */
/* ... @@name:sname@@ ...                                                    */
/* <<END REPEAT name>>                                                       */
/*                                                                           */
/* Iterates over the array/object template.name and replaces name:sname with */
/* the value of template.name.sname.                                         */
/* (^^ and %% are also supported)                                            */
/*                                                                           */
/* Inside of REPEAT sections, the current object from the repeated array is  */
/* Available to JS: replacements as RepeatObj                                */
/*                                                                           */
/* The convienience function Nz(value[, valifundefined]) is also available.  */
/* It returns valifindefined if value is undefined, or value if it is.       */
/* valifundefined defaults to the empty string.                              */
/*                                                                           */
/*****************************************************************************/

/* $Id: template.ssjs,v 1.32 2006/02/25 21:39:35 runemaster Exp $ */

template=new Object;
load("sbbsdefs.js");    // UFLAG_G
load("../web/lib/html_themes.ssjs");
load(siteutils);  // Port Check & Logo Check
load(global_defs);  // Global Defs - User Alias and the like.
 
if(http_request.virtual_path=="/nodelist.ssjs")
    load(main_nodelist); // Who's Online Listing
http_reply.fast=true;
    
load(leftnav_nodelist); // Left Side Navigation Node Listing

template.Theme_CSS_File=Themes[CurrTheme].css;
template.server=server;
template.system=system;


function Nz(value, valifundefined)
{
    if(valifundefined==undefined)
        valifundefined='';
    if(value==undefined)
        return(valifundefined);
    return(value);
}

function write_template(filename)  {
    var fname='../web/templates/'+Themes[CurrTheme].theme_dir+'/'+filename;
    if(!file_exists(fname)) {
        fname='../web/templates/'+Themes[DefaultTheme].theme_dir+'/'+filename;
        if(!file_exists(fname))
            fname='../web/templates/'+Themes["Default"].theme_dir+'/'+filename;
    }
    var inc=new File(fname);
    if(!inc.open("r",true,1024)) {
        horrible_error("!Error " + inc.error + " opening template file: "+ fname);
    }
    var file=inc.read();
    inc.close();
    var offset;
    while((offset=file.search(/<<REPEAT .*?>>/))>-1) {
        parse_regular_bit(file.substr(0,offset),'',template);
        file=file.substr(offset);
        var m=file.match(/^<<REPEAT (.*?)>>([\x00-\xff]*?)<<END REPEAT \1>>/);
        if(m.index>-1) {
            for(obj in template[m[1]]) {
                parse_regular_bit(m[2],m[1],template[m[1]][obj]);
            }
            file=file.substr(m[0].length);
        }
    }
    parse_regular_bit(file, "", template);
}

function parse_regular_bit(bit, objname, RepeatObj) {
 var offset;
 var i=0;
 while((offset=bit.search(/([%^@])\1(.*?)\1\1/))>-1) {
    write(bit.substr(0,offset));
    bit=bit.substr(offset);
    var m=bit.match(/^([%^@])\1([\x00-\xff]*?)\1\1/);
    if(m.index>-1) {
        if(m[2].substr(0,3)=='JS:') {
            var val=eval(m[2].substr(3));
            if(val!=undefined)
                write(escape_match(m[1],val));
        }
        else if(m[2].substr(0,objname.length+1)==objname+':') {
            if(RepeatObj[m[2].substr(objname.length+1)]!=undefined)
                write(escape_match(m[1],RepeatObj[m[2].substr(objname.length+1)]));
        }
        else if((i=m[2].search(/:/))>0) {
            if(template[m[2].substr(0,i)]!=undefined) {
                if(template[m[2].substr(0,i)][m[2].substr(i+1)]!=undefined)
                    write(escape_match(m[1],template[m[2].substr(0,i)][m[2].substr(i+1)]));
            }
        }
        else {
            if(template[m[2]]!=undefined)
                write(escape_match(m[1],template[m[2]]));
        }
    }
    bit=bit.substr(m[0].length);
 }
 write(bit);
}

function escape_match(start, exp)  {
    if(exp==undefined)
        exp='';
    exp=exp.toString();
    if(start=="%")
        exp=html_encode(exp,false,false,false,false);
    if(start=="^")
        exp=encodeURIComponent(exp);
    return(exp);
}

function error(message)  {
    template.title="Error";
    template.err=message;
    write_template("header.inc");
    write_template("error.inc");
    write_template("footer.inc");
    exit();
}

function horrible_error(message) {
    write("<html><head><title>ERROR</title></head><body><p>"+message+"</p></body></html>");
    exit();
}

function regex_escape(str)
{
    str=str.replace(/([\\\^\$\*\+\?\.\(\)\{\}\[\]])/g,"\\$1");
    return(str);
}

 
