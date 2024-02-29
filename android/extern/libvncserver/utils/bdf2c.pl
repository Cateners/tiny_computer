#!/usr/bin/perl

@encodings=();
for($i=0;$i<256*5;$i++) {
  $encodings[$i]="0";
}

$out="";
$counter=0;
$fontname="default";

$i=0;
$searchfor="";
$nullx="0x";

while(<>) {
  if(/^FONT (.*)$/) {
    $fontname=$1;
    $fontname=~y/\"//d;
  } elsif(/^ENCODING (.*)$/) {
    $glyphindex=$1;
    $searchfor="BBX";
    $dwidth=0;
  } elsif(/^DWIDTH (.*) (.*)/) {
    $dwidth=$1;
  } elsif(/^BBX (.*) (.*) (.*) (.*)$/) {
    ($width,$height,$x,$y)=($1,$2,$3,$4);
    @encodings[$glyphindex*5..($glyphindex*5+4)]=($counter,$width,$height,$x,$y);
    if($dwidth != 0) {
      $encodings[$glyphindex*5+1]=$dwidth;
    } else {
      $dwidth=$width;
    }
    $searchfor="BITMAP";
  } elsif(/^BITMAP/) {
    $i=1;
  } elsif($i>0) {
    if($i>$height) {
      $i=0;
      $out.=" /* $glyphindex */\n";
    } else {
      if(int(($dwidth+7)/8) > int(($width+7)/8)) {
	$_ .= "00"x(int(($dwidth+7)/8)-int(($width+7)/8));
      }
      $_=substr($_,0,(int(($dwidth+7)/8)*2));
      $counter+=(int(($dwidth+7)/8));
      s/(..)/$nullx$1,/g;
      $out.=$_;
      $i++;
    }
  }
}

print "unsigned char " . $fontname . "FontData[$counter]={\n" . $out;
print "};\nint " . $fontname . "FontMetaData[256*5]={\n";
for($i=0;$i<256*5;$i++) {
  print $encodings[$i] . ",";
}
print "};\nrfbFontData " . $fontname . "Font={" .
  $fontname . "FontData, " . $fontname . "FontMetaData};\n";
