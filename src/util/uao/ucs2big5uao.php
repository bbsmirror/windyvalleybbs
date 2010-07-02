<?php
/****************************************************
  Maple-itoc RSS 訂閱器 Big5 補完   
 ****************************************************
 作者:  renn999 <AT> bbs.ccns.ncku.edu.tw
        http://renn999.twbbs.org
 
 USAGE:ucs2big5( $str );

 對於某些 iconv 沒有被 patch 過的主機, 可以擴充其編碼 
   
 Big5 table(big5.txt)
 Powered by Ztrem
 http://zhouer.org/ZTerm/

 ****************************************************/

function ucs2big5($ucs_str) {
    $fp = fopen( '/home/bbs/src/util/uao/big5.txt', 'r' );
    $len = strlen($ucs_str);
    unset($big5_str);
    
    $x = 0;
    for( $i = 0 ; $i < $len ; $i++ ) {
        $b1 = ord($ucs_str[$i]);
        if( $b1 < 0x80 ) {
            $big5_str[$x++] = chr($b1);
        }
        elseif( $b1 >= 224 ) {
            #3code UTF-8
            $b1 -= 224;
            $b2 = ord($ucs_str[++$i]) - 128;
            $b3 = ord($ucs_str[++$i]) - 256;
            $ucs_code = $b1 * 4096 + $b2 * 64 + $b3;
            fseek( $fp, $ucs_code * 2);
            $big5_code = fread( $fp, 2 );
            $big5_str[$x++] = $big5_code[0];
            $big5_str[$x++] = $big5_code[1];
        }
        elseif( $b1 >= 192 ) {
            #2code UTF-8
            $b1 -= 192;
            $b2 = ord($ucs_str[++$i]) - 256;
            $ucs_code = $b1 * 64 + $b2 ;
            fseek( $fp, $ucs_code * 2 );
            $big5_code = fread( $fp, 2 );
            $big5_str[$x++] = $big5_code[0];
            $big5_str[$x++] = $big5_code[1];
        }else{
            $big5_str[$x++] = '?';
        }
    }

    fclose($fp);
    if(isset($big5_str)) {
        return join( '', $big5_str);
    }
}
?>
