#!perl
#   24-Sep-2005 Stephen [kiwin] PALM   wmm@kiwin.com

####################################################################
#
# Copyright (C) 2004 Stephen [kiwin] PALM
#
# License is granted to Wi-Fi Alliance members and designated
# contractors for exclusive use in testing of Wi-Fi equipment.
# This license is not transferable and does not extend to non-Wi-Fi
# applications.
#
# Derivative works are authorized and are limited by the
# same restrictions.
#
# Derivatives in binary form must reproduce the
# above copyright notice, the name of the author "Stephen [kiwin] Palm",
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# The name of the author may not be used to endorse or promote
# products derived from this software without specific prior
# written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
####################################################################

require "ctime.pl";

  $dut=$ARGV[0];
  $run="MASA_out_".$dut;

  $out=$run.".html";
  $outdir=".";

open( OUT, ">$out" ) || die "Can't open file: $out html out\n";

printf OUT "<HTML>\n";
printf OUT "<HEAD><TITLE>WMM U-APSD $run </title></head>\n";
printf OUT "<BODY bgcolor=white>\n";
printf OUT "<H1>WMM U-APSD $run</H1>\n";

printf OUT "Page and script contact: <a href=\"mailto:tech\@wi-fi.org\">Wi-Fi Tech</a> <br>\n";
printf OUT "Concept Copyright (C) 2004,2005: <a href=http://kiwin.com>Stephen [kiwin] Palm</a> <br>\n";
printf OUT "Page generation: %s <BR>\n", &ctime(time);
printf OUT "MASA (MA Analysis and Summary Application) Script: %s <BR>\n", $0;
#printf OUT "WMM<superscript>TM</superscript> testplan version: 1.1<br>\n";
printf OUT "<br>The following table summarizes all APSD tests in this directory. <br>A green S is a SUCCESS test.<BR>\n", $0;
printf OUT "A red F indicates a FAILURE test<BR>\n", $0;
printf OUT "An orange I indicates a test that failed due to not completing the test.<BR>\n", $0;
$numdocs = 0;

opendir(DIR, $outdir);
foreach $i ( sort(readdir(DIR)) ) {
    if ( $i =~ /.bsc$/ ) {
        print "Processing $i ... ";
        if ( -e $i.'.html' ) {
          print "HTML exists\n";
        }
        else {
          $pcmd = "perl ma.pl ".$i;
          print " converting with cmd: ", $pcmd;
          print " \n ";
          system( $pcmd );
          if ( $i =~ /M\.W/ ) {
            $pcmd = "perl ma.pl ".$i."  apts_seq_M_W1.txt";
            print " W1 converting with cmd: ", $pcmd;
            print " \n ";
            system( $pcmd );
            $pcmd = "perl ma.pl ".$i."  apts_seq_M_W2.txt";
            print " W2 converting with cmd: ", $pcmd;
            print " \n ";
            system( $pcmd );
          }
        }
    } # if BSC file
} # foreach file
closedir( DIR );


printf OUT "<Table border=1>\n";
printf OUT "<TR><TH>Filename<TH>pgen<TH>Succ.<TH>Fail<TH>Incomp.<TH>#</tr>\n";

opendir(DIR, $outdir);
foreach $i ( sort(readdir(DIR)) ) {
    if ( $i =~ /.html/ ) {
       print "$i \n";
       $numdocs += 1;
       $numlines = 0;
       $failure = 0;
       $success = 0;
       $incomplete = 0;
       $testplan = "";
       $file_html  = $i;
       open( TC_FILE, $file_html ) || print "Can't open file: $file_html\n";


       while (<TC_FILE>) {

         $numlines += 1;
         $line = $_;
         if ( $line =~ /FAILURE/i )  { $failure=1; }
         if ( $line =~ /SUCCESS/i )  { $success=1; }
         if ( $line =~ /Incomplete/i )  { $incomplete=1; }

       } # while
       close( TC_FILE );

       ($ap, $sta, $test, $date, $time ) = split( '-', $i );
       $time =~ s/\.bsc.html//ig;
       printf OUT "<TR>\n";

       printf OUT "<TD><a href=$file_html>$i</a>\n";
       printf OUT "<TD>$test\n";
       if ( $success) { printf OUT "<TD><font color=darkgreen>S</font>\n"; }
       else { printf OUT "<TD> \n"; }
       if ( $failure) { printf OUT "<TD><font color=red>F</font>\n"; }
       else { printf OUT "<TD> \n"; }
       if ( $incomplete ) { printf OUT "<TD><font color=orange>I</font>\n"; }
       else { printf OUT "<TD> \n"; }

       if ( $i =~ /B\.Z/ )  { $testplan="4.2";             $tp402s+=$success; }
       if ( $i =~ /B\.H/ )  { $testplan="4.3";             $tp403s+=$success; }
       if ( $i =~ /B\.D/ )  { $testplan="4.4";             $tp404s+=$success; }
       if ( $i =~ /A\.J/ )  { $testplan="4.5";             $tp405s+=$success; }
       if ( $i =~ /B\.M/ )  { $testplan="4.6";             $tp406s+=$success; }
       if ( $i =~ /L\.1/ )  { $testplan="4.7 or 5.7";      $tp407s+=$success; $tp507s+=$success; }
       if ( $i =~ /A\.Y/ )  { $testplan="4.8";             $tp408s+=$success; }
       if ( $i =~ /M\.Y/ )  { $testplan="4.9";             $tp409s+=$success; }
       if ( $i =~ /M\.V/ )  { $testplan="4.10";            $tp410s+=$success; }
       if ( $i =~ /M\.U/ )  { $testplan="4.11";            $tp411s+=$success; }
       if ( $i =~ /A\.U/ )  { $testplan="4.12";            $tp412s+=$success; }
       if ( $i =~ /M\.L/ )  { $testplan="5.1";             $tp501s+=$success; }
       if ( $i =~ /M\.D/ )  { $testplan="5.2";             $tp502s+=$success; }
       if ( $i =~ /B\.B/ )  { $testplan="5.3";             $tp503s+=$success; }
       if ( $i =~ /B\.K/ )  { $testplan="5.4";             $tp504s+=$success; }
       if ( $i =~ /B\.W/ )  { $testplan="5.5";             $tp505s+=$success; }
       if ( $i =~ /M\.B/ )  { $testplan="5.8";             $tp508s+=$success; }
       if ( $i =~ /M\.K/ )  { $testplan="5.9";             $tp509s+=$success; }
       if ( $i =~ /M\.W/ )  { $testplan="5.10";            $tp510s+=$success; }

       printf OUT "<TD>$testplan\n";

   } # if HTML file
} # foreach file
printf OUT "</Table>\n";

printf OUT "Total Number of Runs = $numdocs<p><br>\n";
printf OUT "The following table totals the number of tests that were SUCCESSFUL.<BR>\n", $0;

printf OUT "<Table border=1>\n";
printf OUT "<TR><TH>APUT</tr>\n";
printf OUT "<tr><TH> Sec. <TH> # <TH> Successes \n";
printf OUT "<tr><TD> 4.1  <TD> N/A <TD> manual \n";
printf OUT "<tr><TD> 4.2  <TD> B.Z <TD> $tp402s \n";
printf OUT "<tr><TD> 4.3  <TD> B.H <TD> $tp403s \n";
printf OUT "<tr><TD> 4.4  <TD> B.D <TD> $tp404s \n";
printf OUT "<tr><TD> 4.5  <TD> A.J <TD> $tp405s \n";
printf OUT "<tr><TD> 4.6  <TD> B.M <TD> $tp406s \n";
printf OUT "<tr><TD> 4.7 and 5.7 <TD> L.1 <TD> $tp407s \n";
printf OUT "<tr><TD> 4.8  <TD> A.Y <TD> $tp408s \n";
printf OUT "<tr><TD> 4.9  <TD> M.Y <TD> $tp409s \n";
printf OUT "<tr><TD> 4.10 <TD> M.V <TD> $tp410s \n";
printf OUT "<tr><TD> 4.11 <TD> M.U <TD> $tp411s \n";
printf OUT "<tr><TD> 4.12 <TD> A.U <TD> $tp412s \n";
printf OUT "<Tr> \n";
printf OUT "<TR><TH>STAUT</tr>\n";
printf OUT "<tr><TH> Sec. <TH> # <TH> Successes \n";
printf OUT "<tr><TD> 5.1  <TD> M.L <TD> $tp501s \n";
printf OUT "<tr><TD> 5.2  <TD> M.D <TD> $tp502s \n";
printf OUT "<tr><TD> 5.3  <TD> B.B <TD> $tp503s \n";
printf OUT "<tr><TD> 5.4  <TD> B.K <TD> $tp504s \n";
printf OUT "<tr><TD> 5.5  <TD> B.W <TD> $tp505s \n";
printf OUT "<tr><TD> 5.6  <TD> N/A <TD> manual \n";
#   printf OUT "<tr><TD> 5.7  <TD> L.1 <TD> $tp507s \n";
printf OUT "<tr><TD> 5.8  <TD> M.B <TD> $tp508s \n";
printf OUT "<tr><TD> 5.9  <TD> M.K <TD> $tp509s \n";
printf OUT "<tr><TD> 5.10 <TD> M.W <TD> $tp510s \n";
printf OUT "</Table>\n";

# Trailers

printf OUT "<HR width=50%><P>\n";
printf OUT "<P>\n";
printf OUT "This page is automatically generated by a <A href=http://www.perl.com>PERL</A> script\n";
printf OUT "</body></html>\n";
close( OUT );
closedir( DIR );

exit;






