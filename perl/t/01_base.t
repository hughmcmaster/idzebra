#!perl -Tw
# =============================================================================
# $Id: 01_base.t,v 1.5 2003-03-05 14:15:07 pop Exp $
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

use Test::More tests=>10;

# ----------------------------------------------------------------------------
# Session opening and closing
BEGIN {
    use_ok('IDZebra');
    IDZebra::logFile("test.log");
    use_ok('IDZebra::Session'); 
}

use pod;
# ----------------------------------------------------------------------------
# Just to be sure...
mkdir ("demo/tmp", 0750);
mkdir ("demo/lock", 0750);
mkdir ("demo/register", 0750);
mkdir ("demo/shadow", 0750);

# ----------------------------------------------------------------------------
# Session opening and closing
my $sess = IDZebra::Session->new(configFile => 'demo/zebra.cfg');
isa_ok($sess,"IDZebra::Session");

$sess->open();
ok(defined($sess->{zh}), "Zebra handle opened");

$sess->close();
ok(!defined($sess->{zh}), "Zebra handle closed");

$sess = IDZebra::Session->open(configFile => 'demo/zebra.cfg',
			       groupName  => 'demo1');
isa_ok($sess,"IDZebra::Session");
ok(defined($sess->{zh}), "Zebra handle opened");

# ----------------------------------------------------------------------------
# Record group tests
ok(($sess->group->{databaseName} eq "demo1"),"Record group is selected");

$sess->group(groupName => 'demo2');

ok(($sess->group->{databaseName} eq "demo2"),"Record group is selected");

# ---------------------------------------------------------------------------
# Transactions
$sess->begin_trans(TRANS_RO);
eval {$sess->begin_trans(TRANS_RW);};
ok (($@ ne ""), $@);
$sess->end_trans;
$sess->end_trans;




# ----------------------------------------------------------------------------
# Close session


$sess->close;

