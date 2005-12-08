#!perl
# =============================================================================
# $Id: 08_scan.t,v 1.2 2004-07-28 08:15:47 adam Exp $
#
# Perl API header
# =============================================================================
BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir 't' if -d 't';
    }
    push (@INC,'demo','blib/lib','blib/arch');
}

use strict;
use warnings;

use Test::More tests => 17;

# ----------------------------------------------------------------------------
# Session opening and closing
BEGIN {
    use IDZebra;
    unlink("test08.log");
    IDZebra::logFile("test08.log");
#  IDZebra::logLevel(15);
    use_ok('IDZebra::Session'); 
    use_ok('pod');
}


# ----------------------------------------------------------------------------
# Session opening and closing
my $sess = IDZebra::Session->open(configFile => 'demo/zebra.cfg',
				  groupName => 'demo1');

$sess->databases('demo1');

our $filecount = 8;
# -----------------------------------------------------------------------------
# Scan titles in multiple databases

my $sl1 = $sess->scan(expression => "\@attr 1=4 \@attr 6=2 a",
		      databases => [qw(demo1 demo2)]);

&test_list($sl1,$filecount, $filecount*2,1);
# -----------------------------------------------------------------------------
# Scan titles in a single and default database
my $sl2 = $sess->scan(expression => "\@attr 1=4 \@attr 6=2 a");
&test_list($sl2,$filecount, $filecount,1);


# -----------------------------------------------------------------------------
# Scan long list, with position...
my $sl3 = $sess->scan(expression  => "\@attr 1=1016 a");

my @entries = $sl3->entries(position    => 5,
			    num_entries => 10000);

my $count = $#entries + 1;
ok (($sl3->errCode == 0),"scan successful");
ok (($sl3->num_entries == $count),"fetched $count entries");
my $i = 1;
my $posok = 1;
foreach my $se (@entries) {
    $posok = 0 if ($se->position != $i); 
    $i++;
}
ok (($posok),"position of each term");


# -----------------------------------------------------------------------------
# Scan error
eval {my $sl4 = $sess->scan(expression => "\@attr 1=9999 a");};
ok (($@ ne ""),"Wrong scan die");
ok (($sess->errCode != 0), 
    "Error reported in session: ".$sess->errCode.
    " (". $sess->errString. ")");


# ----------------------------------------------------------------------------
# Close session
$sess->close;

# ============================================================================
sub test_list {
    my ($sl, $ecount, $occ, $offset) = @_;
    my @entries = $sl->entries();
    my $count = $#entries + 1;
    ok (($sl->errCode == 0),"scan successfull");
    ok (($sl->num_entries == $ecount),
	"number of entries is ".$sl->num_entries);
    ok (($count == $sl->num_entries),"fetched $count entries");
    
    my $occcount=0; 
    my $posok = 1;
    my $i = $offset;
    foreach my $se (@entries) {
	$occcount += $se->occurrences();
	$posok = 0 if ($se->position != $i); 
	$i++;
    }
    
    ok ($occcount == $occ,"occurrences: $occcount");
    ok (($posok),"position of each term");
}