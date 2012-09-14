//==================================================
//修 改 者：fly
//修改时间：2009-11-13
//修 改 点：增加显示提示信息的函数'show_message'
//==================================================
//修 改 者：fly
//修改时间：2009-11-17
//修 改 点：在函数'setSendTo'增加显示和影藏Email输入框的代码
//==================================================

/**
 * 作者: fly
 * 时间: 2009-11-13
 * 作用: 显示提示信息
 * 参数: exportType   导出的类型, 0为WORD, 1为HTML
 * 参数: language     语言, 0为中文, 1为英文
 */
function show_message(exportType, language) {
  var show_message = '';
  if ('0' == exportType) {
    if ('0' == language) {  //中文的word导出
      show_message = '注： 支持word2003及以上版本';
    }
    else if ('1' == language) {  //英文的word导出
      show_message = 'Note: Support Word 2003 and above';
    }

    $('#show_message').html(show_message);
  }
  else if (1 == exportType) {
    if ('0' == language) {  //中文的html导出
      show_message = '注： 支持IE浏览器，暂不支持firefox浏览器';
    }
    else if ('1' == language) {  //英文的html导出
      show_message = 'Note: Support IE browser only';
    }

    $('#show_message').html(show_message);
  }
  $('#show_message').css({'display':'block','color': '#F67214'});
}
function cv_export( isEnglish,DOMAIN_MY,resumeID )
{
	if ( !window.pop ) {
		var Temp_Sel_Param = {
			openType: 2 , //居中定位
			filterParam: {} , //滤镜层设置
			createIfr:false
		}
		window.pop = new ExtZzLayer( Temp_Sel_Param );
	}
	
	var action_url = DOMAIN_MY+"/cv/cv_export.php?isEnglish="+isEnglish+"&act=select&ReSumeID="+resumeID+"&rand="+Math.random();
	
	$.get( action_url,  function(result) {
		pop.setContent( result );
		pop.setCloseNode( 'window_close' );
		pop.open();
	} );
}

function setSendTo( value )
{
  //add by fly on 2009-11-17 start
  if ('' != value) {
    $('#email').css({'display':'inline'});
  }
  else {
    $('#email').css({'display': 'none'});
  }
  //add by fly on 2009-11-17 end
	$("#email").val(value);
}

function checkExportSet( isEnglish )
{
	var ck_type = $(":radio[name=type]:checked").val();
	
	if( !ck_type )
	{
		if( isEnglish == 1 )
		{
			alert('Please select export format!');
		}
		else
		{
			alert('请选择导出格式！');
		}
		return false;
	}
	
	var ck_sendto = $(":radio[name=sendto]:checked").val();
	
	if( !ck_sendto )
	{
		if( isEnglish == 1 )
		{
			alert('Please select Download Method!');
		}
		else
		{
			alert('请选择下载方式！');
		}
		return false;
	}
	
	if( ck_sendto == 'mail' )
	{
		var email = $("#email").val();
		
		if( isEnglish == 1 )
		{
			if( !chkUserEmailFormat( email,'EN' ) )
			{
				return false;
			}
		}
		else
		{
			if( !chkUserEmailFormat( email,'CN' ) )
			{
				return false;
			}
		}
	}
	
	pop.close();
	return true;
}