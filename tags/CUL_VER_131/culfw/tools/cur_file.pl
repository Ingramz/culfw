#!/usr/bin/perl

use strict;
use warnings;

my $debug = 0;

if(@ARGV != 3 || ($ARGV[0] ne "-r" && $ARGV[0] ne "-w")) {
  die("Usage: cur_file.pl {-r|-w} filename <dur-device>\n");
}

open(DEV, "+<$ARGV[2]") || die("Can't open $ARGV[2]: $!\n");

###################################################
my $buf;
for(;;) {                       # Drain input
  my $rin = "";
  vec($rin, fileno(DEV), 1) = 1;
  my $nfound = select($rin, undef, undef, 0.3);
  last if($nfound <= 0);
  sysread(DEV, $buf, 1);
}
   

printf STDERR "Drained input\n" if($debug);

###################################################
if($ARGV[0] eq "-r") {

  open(FH, ">$ARGV[1]") || die("Can't open $ARGV[1]: $!\n");
 
  syswrite(DEV, "r$ARGV[1]\r\n");       # Send the filename
  sysread(DEV, $buf, 1);                # Check if its found
  if($buf =~ m/^X/) {
    die("CUR: File not found\n");
  }
  my $off = 1;
  while(length($buf) != 10) {
    sysread(DEV, $buf, 10-$off, $off);  # Read length (8byte hex)+\r\n
  }
  $buf =~ s/[\r\n]//g;
  my $len = hex($buf);
  $buf = ""; $off = 0;

  printf STDERR "Got length: $len\n" if($debug);

  while($off < $len) {
    my $chunklen += sysread(DEV, $buf, $len-$off); # Read data
    last if(!$chunklen);
    syswrite(FH, $buf);
    printf STDERR "Got: $chunklen\n" if($debug);
    $off += $chunklen;
  }

  printf STDERR "Finished: got $off out of $len\n" if($debug);


} else {

###################################################

  open(FH, "<$ARGV[1]") || die("Can't open $ARGV[1]: $!\n");
  my $buf = join("", <FH>);
  my $len = length($buf);
 
  syswrite(DEV, sprintf("w%08X%s\r\n", $len, $ARGV[1])); 
  my $ret;
  sysread(DEV, $ret, 1);                # Check if it can be written
  if($ret =~ m/^X/) {
    die("CUR: Cant write file\n");
  }
  my $off = 1;
  while(length($ret) != 10) {
    sysread(DEV, $ret, 10-$off, $off);  # Read length (8byte hex)+\r\n
  }
  $ret =~ s/[\r\n]//g;
  my $len2 = hex($ret);
  if($len != $len2) {
    die("Strange length received: $len2 instead of $len\n");
  }

  # Sleep time:Max write speed to the flash is 26kB, write buffer is 32 byte.
  # 1/(26000/32) = 0.00123 sec
  $off = 0;
  while($off < $len) {
    my $mlen = ($len-$off) > 32 ? 32 : ($len-$off);
    $off += syswrite(DEV, $buf, $mlen, $off); # Write data
    select(undef, undef, undef, 0.001);
  }

}

exit(0);
