#!/usr/bin/perl
# 
use DBI;
use strict;
use warnings;
use Socket;
use Time::HiRes;
require 'sys/ioctl.ph'; # ioctl($socket)(Linux)
use POSIX qw(:signal_h); # used for set timeout
use DateTime;
# GLOBLE CONFIG
my $host= get_interface_address('bond0');
my $port = "3306";

my $timeout = 15;
my $username = "";
my $password = '';
my $sql= "select count(*) from mysql.user";
my $db= "mysql";

# END GLOBLE CONFIG
sub HiRestime{
	my $dt = DateTime->from_epoch(
	    epoch     => Time::HiRes::time,
	    time_zone => 'local',
	);
	return $dt->strftime("%H:%M:%S.%5N"); 
}

# log scripts start time
my $starttime = HiRestime;

# get interface ip
sub get_interface_address
{
        my ($iface) = @_;
        my $socket;
        socket($socket, PF_INET, SOCK_STREAM, (getprotobyname('tcp'))[2]) || die "unable to create a socket: $!\n";
        my $buf = pack('a256', $iface);
        if (ioctl($socket, SIOCGIFADDR(), $buf) && (my @address = unpack('x20 C4', $buf)))
        {
                return join('.', @address);
        }
        return undef;
}

# sub log
my $logdir="/var/log/";
# the orig source http://jeredsutton.com/2010/07/18/simple-perl-logging-subroutine/
sub logit{
    my $s = shift;
    my ($logsec,$logmin,$loghour,$logmday,$logmon,$logyear,$logwday,$logyday,$logisdst)=localtime(time);
    my $logtimestamp = sprintf("%4d-%02d-%02d %02d:%02d:%02d",$logyear+1900,$logmon+1,$logmday,$loghour,$logmin,$logsec);
    $logmon++;
    my $logfile="$logdir/mysqlchk-$logmon-$logmday.log";
    my $fh;
    open($fh, '>>', "$logfile") or die "$logfile: $!";
    print $fh "$logtimestamp $s\n";
    close($fh);
}

# set sighandle 
my $mask = POSIX::SigSet->new( SIGALRM );
my $action = POSIX::SigAction->new(
    sub { die "Query timeout\n" },        
    $mask,
);

my $oldaction = POSIX::SigAction->new();
sigaction( SIGALRM, $action, $oldaction );

my $dbh;
my $ret;
# a timer
my $start_time = [Time::HiRes::gettimeofday()];
eval {
   eval {
       alarm($timeout); # seconds before time out
        eval {
			$dbh = DBI->connect(
	       			"DBI:mysql:database=$db;host=$host;post=$port;mysql_read_timeout=8;
	       			mysql_connect_timeout=$timeout",
	       			"$username", "$password",
	                   {'RaiseError' => 1, 'PrintError' => 0});
     
		 	my $sth = $dbh->prepare($sql) ; 
	     	$sth->execute();
	     	my $status = $sth->fetchrow_hashref();

	     	foreach( keys %$status ) {
			 $ret = "$_ = $status->{$_}" if $status->{$_};
			 #logit("$_ = $status->{$_}") if $status->{$_};
				 
	     	};

	     	$sth->finish();
	     	# Disconnect from the database.
	     	$dbh->disconnect();			
		};	 
		if ($@) {
			logit("$@");
			exit 1;
		}
   };
   alarm(0); # cancel alarm (if connect worked fast)
   logit("$@") if $@;
   #print"$@" if $@;
   
   exit 1 if $@;
};
if ($@) {
	logit("$@");
	#print"$@" if $@;
	exit 1;
};
my $spend = Time::HiRes::tv_interval($start_time);
my $endtime = HiRestime;
logit("scripts start at $starttime end at $endtime, SQL return \"$ret\" And SQL Query spend $spend seconds");
