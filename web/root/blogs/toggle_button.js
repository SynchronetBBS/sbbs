<!--

function expand_item(itemID, buttonID)
{
	var item = document.getElementById(itemID);
	var button = document.getElementById(buttonID);

	item.style.display = "block";
	button.src=image_path+"minus.gif";
}

function collapse_item(itemID, buttonID)
{
	var item = document.getElementById(itemID);
	var button = document.getElementById(buttonID);

	item.style.display = "none";
	button.src=image_path+"plus.gif";
}

function toggle_display(itemID, buttonID)
{
	var item = document.getElementById(itemID);

	if (item.style.display != "none")
		collapse_item(itemID, buttonID);
	else
		expand_item(itemID, buttonID);
}

//-->
