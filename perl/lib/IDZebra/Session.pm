#!/usr/bin/perl
# ============================================================================
# Zebra perl API header
# =============================================================================
use strict;
# ============================================================================
package IDZebra::Session;
use IDZebra;
use IDZebra::Logger qw(:flags :calls);
#use IDZebra::Repository;
use IDZebra::Resultset;
use Scalar::Util;
use Carp;
use strict;
our @ISA = qw(IDZebra::Logger);

1;
# -----------------------------------------------------------------------------
# Class constructors, destructor
# -----------------------------------------------------------------------------
sub new {
    my ($proto, %args) = @_;
    my $class = ref($proto) || $proto;
    my $self = {};
    $self->{args} = \%args;
    
    bless ($self, $class);
    $self->{cql_ct} = undef;
    return ($self);

    $self->{databases} = {};
}

sub start_service {
    my ($self, %args) = @_;

    my $zs;
    unless (defined($self->{zs})) {
	if (defined($args{'configFile'})) {
	    $self->{zs} = IDZebra::start($args{'configFile'});
	} else {
	    $self->{zs} = IDZebra::start("zebra.cfg");
	}
    }
}

sub stop_service {
    my ($self) = @_;
    if (defined($self->{zs})) {
        IDZebra::stop($self->{zs}) if ($self->{zs});    
	$self->{zs} = undef;
    }
}


sub open {
    my ($proto,%args) = @_;
    my $self = {};

    if (ref($proto)) { $self = $proto; } else { 
	$self = $proto->new(%args);
    }

    unless (%args) {
	%args = %{$self->{args}};
    }

    $self->start_service(%args);

    unless (defined($self->{zs})) {
	croak ("Falied to open zebra service");
    }    

    unless (defined($self->{zh})) {
	$self->{zh}=IDZebra::open($self->{zs}); 
    }   

    # Reset result set counter
    $self->{rscount} = 0;

    # This is needed in order to somehow initialize the service
    $self->select_databases("Default");

    # Load the default configuration
    $self->group(%args);
    
    $self->{odr_input} = IDZebra::odr_createmem($IDZebra::ODR_DECODE);
    $self->{odr_output} = IDZebra::odr_createmem($IDZebra::ODR_ENCODE);

    return ($self);
}

sub close {
    my ($self) = @_;

    if ($self->{zh}) {
	while (IDZebra::trans_no($self->{zh}) > 0) {
	    logf (LOG_WARN,"Explicitly closing transaction with session");
	    $self->end_trans;
	}

        IDZebra::close($self->{zh});
	$self->{zh} = undef;
    }
    
    if ($self->{odr_input}) {
        IDZebra::odr_reset($self->{odr_input});
        IDZebra::odr_destroy($self->{odr_input});
	$self->{odr_input} = undef;  
    }

    if ($self->{odr_output}) {
        IDZebra::odr_reset($self->{odr_output});
        IDZebra::odr_destroy($self->{odr_output});
	$self->{odr_output} = undef;  
    }

    $self->stop_service;
}

sub DESTROY {
    my ($self) = @_;
    logf (LOG_LOG,"DESTROY $self");
    $self->close; 

    if (defined ($self->{cql_ct})) {
      IDZebra::cql_transform_close($self->{cql_ct});
    }
}
# -----------------------------------------------------------------------------
# Record group selection  This is a bit nasty... but used at many places 
# -----------------------------------------------------------------------------
sub group {
    my ($self,%args) = @_;
    if ($#_ > 0) {
	$self->{rg} = $self->_makeRecordGroup(%args);
	$self->_selectRecordGroup($self->{rg});
    }
    return($self->{rg});
}

sub selectRecordGroup {
    my ($self, $groupName) = @_;
    $self->{rg} = $self->_getRecordGroup($groupName);
    $self->_selectRecordGroup($self->{rg});
}

sub _displayRecordGroup {
    my ($self, $rg) = @_;
    print STDERR "-----\n";
    foreach my $key qw (groupName 
			databaseName 
			path recordId 
			recordType 
			flagStoreData 
			flagStoreKeys 
			flagRw 
			fileVerboseLimit 
			databaseNamePath 
			explainDatabase 
			followLinks) {
	print STDERR "$key:",$rg->{$key},"\n";
    }
}

sub _cloneRecordGroup {
    my ($self, $orig) = @_;
    my $rg = IDZebra::recordGroup->new();
    my $r = IDZebra::init_recordGroup($rg);
    foreach my $key qw (groupName 
			databaseName 
			path 
			recordId 
			recordType 
			flagStoreData 
			flagStoreKeys 
			flagRw 
			fileVerboseLimit 
			databaseNamePath 
			explainDatabase 
			followLinks) {
	$rg->{$key} = $orig->{$key} if ($orig->{$key});
    }
    return ($rg);
}

sub _getRecordGroup {
    my ($self, $groupName, $ext) = @_;
    my $rg = IDZebra::recordGroup->new();
    my $r = IDZebra::init_recordGroup($rg);
    $rg->{groupName} = $groupName if ($groupName ne "");  
    $ext = "" unless ($ext);
    my $r = IDZebra::res_get_recordGroup($self->{zh}, $rg, $ext);
    return ($rg);
}

sub _makeRecordGroup {
    my ($self, %args) = @_;
    my $rg;

    my @keys = keys(%args);
    unless ($#keys >= 0) {
	return ($self->{rg});
    }

    if ($args{groupName}) {
	$rg = $self->_getRecordGroup($args{groupName});
    } else {
	$rg = $self->_cloneRecordGroup($self->{rg});
    }
    $self->_setRecordGroupOptions($rg, %args);
    return ($rg);
}

sub _setRecordGroupOptions {
    my ($self, $rg, %args) = @_;

    foreach my $key qw (databaseName 
			path 
			recordId 
			recordType 
			flagStoreData 
			flagStoreKeys 
			flagRw 
			fileVerboseLimit 
			databaseNamePath 
			explainDatabase 
			followLinks) {
	if (defined ($args{$key})) {
	    $rg->{$key} = $args{$key};
	}
    }
}
sub _selectRecordGroup {
    my ($self, $rg) = @_;
    my $r = IDZebra::set_group($self->{zh}, $rg);
    my $dbName;
    unless ($dbName = $rg->{databaseName}) {
	$dbName = 'Default';
    }
    if ($self->select_databases($dbName)) {
	croak("Fatal error selecting database $dbName");
    }
}
# -----------------------------------------------------------------------------
# Selecting databases for search (and also for updating - internally)
# -----------------------------------------------------------------------------
sub select_databases {
    my ($self, @databases) = @_;

    my $changed = 0;
    foreach my $db (@databases) {
	next if ($self->{databases}{$db});
	$changed++;
    }

    if ($changed) {

	delete ($self->{databases});
	foreach my $db (@databases) {
	    $self->{databases}{$db}++;
	}

	if (my $res = IDZebra::select_databases($self->{zh}, 
						($#databases + 1), 
						\@databases)) {
	    logf(LOG_FATAL, 
		 "Could not select database(s) %s errCode=%d",
		 join(",",@databases),
		 $self->errCode());
	    return ($res);
	} else {
	    logf(LOG_LOG,"Database(s) selected: %s",join(",",@databases));
	}
    }
    return (0);
}

# -----------------------------------------------------------------------------
# Error handling
# -----------------------------------------------------------------------------
sub errCode {
    my ($self) = @_;
    return(IDZebra::errCode($self->{zh}));
}

sub errString {
    my ($self) = @_;
    return(IDZebra::errString($self->{zh}));
}

sub errAdd {
    my ($self) = @_;
    return(IDZebra::errAdd($self->{zh}));
}

# -----------------------------------------------------------------------------
# Transaction stuff
# -----------------------------------------------------------------------------
sub begin_trans {
    my ($self) = @_;
    IDZebra::begin_trans($self->{zh});
}

sub end_trans {
    my ($self) = @_;
    my $stat = IDZebra::ZebraTransactionStatus->new();
    IDZebra::end_trans($self->{zh}, $stat);
    return ($stat);
}

sub begin_read {
    my ($self) =@_;
    return(IDZebra::begin_read($self->{zh}));
}

sub end_read {
    my ($self) =@_;
    IDZebra::end_read($self->{zh});
}

sub shadow_enable {
    my ($self, $value) = @_;
    if ($#_ > 0) { IDZebra::set_shadow_enable($self->{zh},$value); }
    return (IDZebra::get_shadow_enable($self->{zh}));
}

sub commit {
    my ($self) = @_;
    if ($self->shadow_enable) {
	return(IDZebra::commit($self->{zh}));
    }
}

# -----------------------------------------------------------------------------
# We don't really need that...
# -----------------------------------------------------------------------------
sub odr_reset {
    my ($self, $name) = @_;
    if ($name !~/^(input|output)$/) {
	croak("Undefined ODR '$name'");
    }
  IDZebra::odr_reset($self->{"odr_$name"});
}

# -----------------------------------------------------------------------------
# Init/compact
# -----------------------------------------------------------------------------
sub init {
    my ($self) = @_;
    return(IDZebra::init($self->{zh}));
}

sub compact {
    my ($self) = @_;
    return(IDZebra::compact($self->{zh}));
}

sub update {
    my ($self, %args) = @_;
    my $rg = $self->_update_args(%args);
    $self->_selectRecordGroup($rg);
    $self->begin_trans;
    IDZebra::repository_update($self->{zh});
    $self->_selectRecordGroup($self->{rg});
    $self->end_trans;
}

sub delete {
    my ($self, %args) = @_;
    my $rg = $self->_update_args(%args);
    $self->_selectRecordGroup($rg);
    $self->begin_trans;
    IDZebra::repository_delete($self->{zh});
    $self->_selectRecordGroup($self->{rg});
    $self->end_trans;
}

sub show {
    my ($self, %args) = @_;
    my $rg = $self->_update_args(%args);
    $self->_selectRecordGroup($rg);
    $self->begin_trans;
    IDZebra::repository_show($self->{zh});
    $self->_selectRecordGroup($self->{rg});
    $self->end_trans;
}

sub _update_args {
    my ($self, %args) = @_;
    my $rg = $self->_makeRecordGroup(%args);
    $self->_selectRecordGroup($rg);
    return ($rg);
}

# -----------------------------------------------------------------------------
# Per record update
# -----------------------------------------------------------------------------

sub update_record {
    my ($self, %args) = @_;
    return(IDZebra::update_record($self->{zh},
				  $self->_record_update_args(%args)));
}

sub delete_record {
    my ($self, %args) = @_;
    return(IDZebra::delete_record($self->{zh},
				  $self->_record_update_args(%args)));
}
sub _record_update_args {
    my ($self, %args) = @_;

    my $sysno   = $args{sysno}      ? $args{sysno}      : 0;
    my $match   = $args{match}      ? $args{match}      : "";
    my $rectype = $args{recordType} ? $args{recordType} : "";
    my $fname   = $args{file}       ? $args{file}       : "<no file>";

    my $buff;

    if ($args{data}) {
	$buff = $args{data};
    } 
    elsif ($args{file}) {
	open (F, $args{file}) || warn ("Cannot open $args{file}");
	$buff = join('',(<F>));
	close (F);
    }
    my $len = length($buff);

    delete ($args{sysno});
    delete ($args{match});
    delete ($args{recordType});
    delete ($args{file});
    delete ($args{data});

    my $rg = $self->_makeRecordGroup(%args);

    # If no record type is given, then try to find it out from the
    # file extension;
    unless ($rectype) {
	if (my ($ext) = $fname =~ /\.(\w+)$/) {
	    my $rg2 = $self->_getRecordGroup($rg->{groupName},$ext);
	    $rectype = $rg2->{recordType};
	} 
    }

    $rg->{databaseName} = "Default" unless ($rg->{databaseName});

#    print STDERR "$rectype,$sysno,$match,$fname,$len\n";
    unless ($rectype) {
	$rectype="";
    }
    return ($rg, $rectype, $sysno, $match, $fname, $buff, $len);
}

# -----------------------------------------------------------------------------
# CQL stuff
sub cqlmap {
    my ($self,$mapfile) = @_;
    if ($#_ > 0) {
	if ($self->{cql_mapfile} ne $mapfile) {
	    unless (-f $mapfile) {
		croak("Cannot find $mapfile");
	    }
	    if (defined ($self->{cql_ct})) {
	      IDZebra::cql_transform_close($self->{cql_ct});
	    }
	    $self->{cql_ct} = IDZebra::cql_transform_open_fname($mapfile);
	    $self->{cql_mapfile} = $mapfile;
	}
    }
    return ($self->{cql_mapfile});
}

sub cql2pqf {
    my ($self, $cqlquery) = @_;
    unless (defined($self->{cql_ct})) {
	croak("CQL map file is not specified yet.");
    }
    my $res = "\0" x 2048;
    my $r = IDZebra::cql2pqf($self->{cql_ct}, $cqlquery, $res, 2048);
    unless ($r) {return (undef)};
    $res=~s/\0.+$//g;
    return ($res); 
}


# -----------------------------------------------------------------------------
# Search 
# -----------------------------------------------------------------------------
sub search {
    my ($self, %args) = @_;

    if ($args{cqlmap}) { $self->cqlmap($args{cqlmap}); }

    my $query;
    if ($args{pqf}) {
	$query = $args{pqf};
    }
    elsif ($args{cql}) {
	unless ($query = $self->cql2pqf($args{cql})) {
	    croak ("Invalid CQL query: '$args{cql}'");
	}
    }
    unless ($query) {
	croak ("No query given to search");
    }

    my $rsname = $args{rsname} ? $args{rsname} : $self->_new_setname;

    return ($self->_search_pqf($query, $rsname));
}

sub _new_setname {
    my ($self) = @_;
    return ("set_".$self->{rscount}++);
}

sub _search_pqf {
    my ($self, $query, $setname) = @_;

    my $hits = IDZebra::search_PQF($self->{zh},
				   $self->{odr_input},
				   $self->{odr_output},
				   $query,
				   $setname);

    my $rs  = IDZebra::Resultset->new($self,
				      name        => $setname,
				      recordCount => $hits,
				      errCode     => $self->errCode,
				      errString   => $self->errString);
    return($rs);
}

sub search_cql {
    my ($self, $query, $transfile) = @_;
}


sub search_ccl {
    my ($self, $query, $transfile) = @_;
}

# -----------------------------------------------------------------------------
# Sort
#
# Sorting of multiple result sets is not supported by zebra...
# -----------------------------------------------------------------------------

sub sortResultsets {
    my ($self, $sortspec, $setname, @sets) = @_;

    my @setnames;
    my $count = 0;
    foreach my $rs (@sets) {
	push (@setnames, $rs->{name});
	$count += $rs->{recordCount};  # is this really sure ??? It doesn't 
	                               # matter now...
    }

    my $status = IDZebra::sort($self->{zh},
			       $self->{odr_output},
			       $sortspec,
			       $setname,
			       \@setnames);

    my $errCode = $self->errCode;
    my $errString = $self->errString;

    if ($status || $errCode) {$count = 0;}

    my $rs  = IDZebra::Resultset->new($self,
				      name        => $setname,
				      recordCount => $count,
				      errCode     => $errCode,
				      errString   => $errString);
    
    return ($rs);
}


__END__

=head1 NAME

IDZebra::Session - A Zebra database server session for update and retrieval

=head1 SYNOPSIS

  $sess = IDZebra::Session->new(configFile => 'demo/zebra.cfg');
  $sess->open();

  $sess = IDZebra::Session->open(configFile => 'demo/zebra.cfg');

  $sess->close;

=head1 DESCRIPTION

Zebra is a high-performance, general-purpose structured text indexing and retrieval engine. It reads structured records in a variety of input formats (eg. email, XML, MARC) and allows access to them through exact boolean search expressions and relevance-ranked free-text queries. 

Zebra supports large databases (more than ten gigabytes of data, tens of millions of records). It supports incremental, safe database updates on live systems. You can access data stored in Zebra using a variety of Index Data tools (eg. YAZ and PHP/YAZ) as well as commercial and freeware Z39.50 clients and toolkits. 

=head1 OPENING AND CLOSING A ZEBRA SESSIONS

For the time beeing only local database services are supported, the same way as calling zebraidx or zebrasrv from the command shell. In order to open a local Zebra database, with a specific configuration file, use

  $sess = IDZebra::Session->new(configFile => 'demo/zebra.cfg');
  $sess->open();

or

  $sess = IDZebra::Session->open(configFile => 'demo/zebra.cfg');

where $sess is going to be the object representing a Zebra Session. Whenever this variable gets out of scope, the session is closed, together with all active transactions, etc... Anyway, if you'd like to close the session, just say:

  $sess->close();

This will
  - close all transactions
  - destroy all result sets
  - close the session

In the future different database access methods are going to be available, 
like:

  $sess = IDZebra::Session->open(server => 'ostrich.technomat.hu:9999');

You can also use the B<record group> arguments described below directly when calling the constructor, or the open method:

  $sess = IDZebra::Session->open(configFile => 'demo/zebra.cfg',
                                 groupName  => 'demo');


=head1 RECORD GROUPS 

If you manage different sets of records that share common characteristics, you can organize the configuration settings for each type into "groups". See the Zebra manual on the configuration file (zebra.cfg). 

For each open session a default record group is assigned. You can configure it in the constructor, or by the B<set_group> method:

  $sess->group(groupName => ..., ...)

The following options are available:

=over 4

=item B<groupName>

This will select the named record group, and load the corresponding settings from the configuration file. All subsequent values will overwrite those...

=item B<databaseName>

The name of the (logical) database the updated records will belong to.

=item B<path>

This path is used for directory updates (B<update>, B<delete> methods);
 
=item B<recordId>

This option determines how to identify your records. See I<Zebra manual: Locating Records>

=item B<recordType>

The record type used for indexing. 

=item B<flagStoreData> 

Specifies whether the records should be stored internally in the Zebra system files. If you want to maintain the raw records yourself, this option should be false (0). If you want Zebra to take care of the records for you, it should be true(1). 

=item B<flagStoreKeys>

Specifies whether key information should be saved for a given group of records. If you plan to update/delete this type of records later this should be specified as 1; otherwise it should be 0 (default), to save register space. 

=item B<flagRw>

?

=item B<fileVerboseLimit>

Skip log messages, when doing a directory update, and the specified number of files are processed...

=item B<databaseNamePath>

?

=item B<explainDatabase>

The name of the explain database to be used

=item B<followLinks>              

Follow links when doing directory update.

=back

You can use the same parameters calling all update methods.

=head1 TRANSACTIONS (WRITE LOCKS)

A transaction is a block of record update (insert / modify / delete) procedures. So, all call to such function will implicitly start a transaction, unless one is started by

  $sess->begin_trans;

For multiple per record updates it's efficient to start transactions explicitly: otherwise registers (system files, vocabularies, etc..) are updated one by one. After finishing all requested updates, use

  $stat = $sess->end_trans;

The return value is a ZebraTransactionStatus object, containing the following members as a hash reference:

  $stat->{processed} # Number of records processed
  $stat->{updated}   # Number of records processed
  $stat->{deleted}   # Number of records processed
  $stat->{inserted}  # Number of records processed
  $stat->{stime}     # System time used
  $stat->{utime}     # User time used

=head1 UPDATING DATA

There are two ways to update data in a Zebra database using the perl API. You can update an entire directory structure just the way it's done by zebraidx:

  $sess->update(path      =>  'lib');

This will update the database with the files in directory "lib", according to the current record group settings.

  $sess->update();

This will update the database with the files, specified by the default record group setting. I<path> has to be specified there...

  $sess->update(groupName => 'demo1',
	        path      =>  'lib');

Update the database with files in "lib" according to the settings of group "demo1"

  $sess->delete(groupName => 'demo1',
	        path      =>  'lib');

Delete the records derived from the files in directory "lib", according to the "demo1" group settings. Sounds complex? Read zebra documentation about identifying records.

You can also update records one by one, even directly from the memory:

  $sysno = $sess->update_record(data       => $rec1,
		                recordType => 'grs.perl.pod',
			        groupName  => "demo1");

This will update the database with the given record buffer. Note, that in this case recordType is explicitly specified, as there is no filename given, and for the demo1 group, no default record type is specified. The return value is the system assigned id of the record.

You can also index a single file:

  $sysno = $sess->update_record(file => "lib/IDZebra/Data1.pm");

Or, provide a buffer, and a filename (where filename will only be used to identify the record, if configured that way, and possibly to find out it's record type):

  $sysno = $sess->update_record(data => $rec1,
                                file => "lib/IDZebra/Data1.pm");

And some crazy stuff:

  $sysno = $sess->delete_record(sysno => $sysno);

where sysno in itself is sufficient to identify the record

  $sysno = $sess->delete_record(data => $rec1,
			        recordType => 'grs.perl.pod',
			        groupName  => "demo1");

This case the record is extracted, and if already exists, located in the database, then deleted... 

  $sysno = $sess->delete_record(data       => $rec1,
                                match      => $myid,
                                recordType => 'grs.perl.pod',
			        groupName  => "demo1");

Don't try this at home! This case, the record identifier string (which is normally generated according to the rules set in recordId directive of zebra.cfg) is provided directly....


B<Important:> Note, that one record can be updated only once within a transaction - all subsequent updates are skipped. 

=head1 SEARCHING


=head1 COPYRIGHT

Fill in

=head1 AUTHOR

Peter Popovics, pop@technomat.hu

=head1 SEE ALSO

IDZebra, IDZebra::Data1, Zebra documentation

=cut
