#!/usr/bin/perl
# Copyright (C) 2005 Stephen [kiwin] PALM  wmm@kiwin.com
# All rights reserved

printf "Pgen / analyzer execution script\n";

$ap=$ARGV[0];
$sta=$ARGV[1];
$band=$ARGV[2];
$test=$ARGV[3];
($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) = localtime(time);
$year += 1900 ;
$mon++;
$date = sprintf( "-%04d%02d%02d-%02d%02d%02d", $year, $mon, $mday, $hour, $min, $sec );
$run = "AP_".$ap."-STA_".$sta;

#Added for saving files to test directory J.K 11/17/2005
$testdir = "testfiles";
$dirA = "A";
$dirB = "B";
$dirG = "G";

# Create directory for test files

mkdir ($testdir, 0777);
mkdir ("$testdir/$dirA", 0777);
mkdir ("$testdir/$dirB", 0777);
mkdir ("$testdir/$dirG", 0777);

if($band)
{
    if($band =~ /a/i){
	$band = "A";}
    elsif($band =~ /b/i){
	$band = "B";}
    elsif($band =~ /g/i){
	$band = "G";}
  else{
      printf("You must specify a valid band for testing, use A, B or G. \n");
      printf("Exiting ...\n ");
      exit;
  }
}
else{
    printf("Since no band is specified by you, we assume you are using G band.");
    $band = "G";
}
   

if ( $test ) {
   @tests = ( $test );
}
else {   # Note: only the last line is used
#all ACs trig/delivery enabled
#   @tests = ( 'B.B', 'B.D', 'B.H', 'B.Z', 'B.K', 'B.W', 'A.J' );
#   @tests = ( 'M.D', 'M.L' ); #legacy AP
# partial ACs trig/delivery enabled
#    @tests = ('M.Y', 'M.V', 'M.B', 'M.K', 'M.W');
#    none of the ACs enabled
#    @tests = ('M.U', 'A.U');
   @tests = ( 'M.V' );
#   @tests = ( 'L.1' );
#   @tests = ( 'M.W' );
#    @tests = ('B.M');
#   @tests = ( 'B.D', 'B.W', 'A.J' );
}

foreach $test ( @tests ) {

    ## Wireless Sniffer
    #   change "prism0" to the interface of promiscuous interface, e.g. ath0
#    $acmd = "./APSDSniff/apsdsniff -c -a ath0 -w 500 > ".$run."-".$test.$date.".bsc";
    $acmd = "./APSDSniff/apsdsniff -c -a ath0 -w 500 > ./$testdir/$band/".$run."-".$test.$date.".bsc";
    if ( $test ne 'L.1' ) {
        printf "A cmd: %s\n", $acmd;
	    unless ( $apid = fork ) {
                system( $acmd );
                exit 0;
	    }
    }

    # Sleep 1 second for sniffer to capture hello message	
    sleep 1;

    ## pgen
    if ( $test eq 'L.1' ) {
       $pcmd = "./apts ".$test;
    }
    else {
       $pcmd = "./apts -trace ".$test;
    }
    printf "P cmd: %s\n", $pcmd;
    system( $pcmd );

    ##wait for APSD sniff to complete
    if ( $test ne 'L.1' ) {
        printf "Waiting for capture to complete APID %d \n", $apid;
        waitpid( $apid, 0);
        printf "Wireless capture complete\n";
    }
    else {
#       $acmd = "mv temp.bsc ".$run."-".$test.$date.".bsc";
        $acmd = "mv temp.bsc ./$testdir/$band/".$run."-".$test.$date.".bsc";
       printf "A cmd: %s\n", $acmd;
       system( $acmd );
    }

    ## ma
    printf "Waiting for analyzer to finish.";

    if($test eq 'M.W') {
        $mcmd = "/usr/bin/perl ma.pl ./$testdir/$band/".$run."-".$test.$date.".bsc"." apts_seq_M_W1.txt";
    	printf "MA cmd: %s\n", $mcmd;
    	system( $mcmd );
	
        $mcmd = "/usr/bin/perl ma.pl ./$testdir/$band/".$run."-".$test.$date.".bsc"." apts_seq_M_W2.txt";
    	printf "MA cmd: %s\n", $mcmd;
    	system( $mcmd );
	}
    elsif($test eq 'B.H') {
        $mcmd = "/usr/bin/perl ma.pl ./$testdir/$band/".$run."-".$test.$date.".bsc"." apts_seq_B_H1.txt";
    	printf "MA cmd: %s\n", $mcmd;
    	system( $mcmd );
	
        $mcmd = "/usr/bin/perl ma.pl ./$testdir/$band/".$run."-".$test.$date.".bsc"." apts_seq_B_H2.txt";
    	printf "MA cmd: %s\n", $mcmd;
    	system( $mcmd );
	}
    else {
        $mcmd = "/usr/bin/perl ma.pl ./$testdir/$band/".$run."-".$test.$date.".bsc";
    	printf "MA cmd: %s\n", $mcmd;
    	system( $mcmd );
    }
}

exit( 0 );
