#!/usr/bin/perl

# convert a linux console font (8x16 format) to a font definition
# suitable for processing with LibVNCServer

#if($#ARGV == 0) { exit; }

foreach $i (@ARGV) {
  $fontname = $i;
  $fontname =~ s/\.fnt$//;
  $fontname =~ s/^.*\///g;
  $fontname =~ y/+/_/;

  print STDERR "$i -> $fontname\n";

  open IN, "<$i";
  print STDERR read(IN,$x,4096);
  close(IN);

  open OUT, ">$fontname.h";
  print OUT "unsigned char ".$fontname."FontData[4096+1]={";
  for($i=0;$i<4096;$i++) {
    if(($i%16)==0) {
      print OUT "\n";
    }
    printf OUT "0x%02x,", ord(substr($x,$i));
  }

  print OUT "\n};\nint ".$fontname."FontMetaData[256*5+1]={\n";
  for($i=0;$i<256;$i++) {
    print OUT ($i*16).",8,16,0,0,";
  }

  print OUT "};\nrfbFontData ".$fontname."Font = { ".$fontname."FontData, "
    .$fontname."FontMetaData };\n";

  close OUT;
}
