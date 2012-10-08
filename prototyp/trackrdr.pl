#!/usr/bin/perl

##
## Prototyp for the tracking reader
##
## opens varnishlog and parses the relevant tags
## sends complete data records to the processor
##
## For copyright and licensing notices, see the pod section COPYRIGHT
## below
##

=head1 NAME

trackrdr.pl - read tracking data from the Varnish SHM-log and send
data sets to the processor

=head1 SYNOPSIS

 $ trackrdr.pl [-c config_file]
               [-t [MQ|HTTP]] [-u http_url] [-m mq_url] [-q queue]
               [[-n varnish_logfile] | [-f varnishlog_outputfile]]
               [-v varnish_prefix] [-r max_restarts] [-d]
               [-l logfile] [-p pidfile] [-s status_interval]
    	       [-b [stdbuf_path]:[stdbuf_bufsize]]
               [--help] [--version]

=head1 DESCRIPTION

C<trackrdr.pl> starts an instance of C<varnishlog>, parses its
output for tags relevant to tracking, and sends complete data records
to the processor via HTTP or a message queue.

=head1 CONFIGURATION

By default, C<trackrdr.pl> reads its configuration from
C</etc/trackrdr.conf>. That configuration may be overriden by a file
given with the C<-c> option, and individual values may be specified by
other command line options.

The format of a config file is:

 # Test configuration for the varnish log tracking reader
 transport = mq
 # mq.url = stomp://127.0.0.1:61613
 varnish.prefix = /usr/local
 log = /tmp/trackrdr.log

There is a command line option corresponding to each configuration
variable, so these are all documented in the section L</OPTIONS>.

=head1 OPTIONS

A choice for B<-t/transport> is B<required>. No command line options
are required, defaults are described in the following.

=over

=item B<-t [HTTP|MQ]>

Config variable B<transport>. Whether HTTP or a message queue is used
to submit data to the processor (no default).

=item B<-c config_file>

A configuration file for C<trackrdr.pl>. If C</etc/trackrdr.conf>
exists, then the configuration is read from there first. Values in the
file given with B<-c> may override values from C</etc/trackrdr.conf>,
and values given with command line options may in turn override values
from any config file.

=item B<-u processor_url>

Config variable B<http.url>. URL of the processor application, to
which data records are submitted if HTTP transport is chosen. Default:
C<http://localhost/ts-processor/httpProcess>

=item B<-m mq_url>

Config variable B<mq.url>. URL used to connect to a message queue
broker, if MQ transport is chosen. Default: C<stomp://127.0.0.1:61613>

=item B<-q queue>

Config variable B<queue>. Name of the queue to which messages are
sent, if MQ transport is chosen. Default: C<lhotse/tracking/rdr2proc>

=item B<-n varnish_logfile>

Config variable B<varnish.name>. The "varnish name" indicating the
mmap'd log file used by C<varnishd> and C<varnishlog>, used for the
C<-n> option to start C<varnishlog>. By default, C<varnishlog> is
started without an C<-n> option (so the default for C<varnishlog>
holds).

=item B<-f varnishlog_outputfile>

Config variable B<varnishlog.dump>. Path of a file created by
redirecting standard output of C<varnishlog>, useful for debugging
purposes. If specified, C<trackrdr.pl> reads from this file, instead
of reading live C<varnishlog> output. The options B<-n> and B<-f> are
mutually exclusive. By default, the default choice for B<-n> is
assumed (read the SHM log at the default location for C<varnishlog>).

=item B<-v varnish_prefix>

Config variable B<varnish.prefix>. Installation directory for varnish,
default: C</var/opt/varnish>

=item B<-r max_restarts>

Config variable B<restarts>. Maximum number of restarts for the child
process, or 0 for unlimited, default 0

=item B<-d>

Config variable B<debug>. Switches on debug mode, off by default. (In
a config file, set B<debug> to B<true> or B<false>.)

=item B<-l logfile>

Config variable B<log>. Log file for status, warning, debug and error
messages. By default, status and debug messages are written to
C<STDOUT>, and warnings and error messages are written to C<STDERR>.

=item B<-p pidfile>

Config variable B<pid.file>. File in which the process ID of the
parent process is stored. To stop the script, it suffices to send a
C<TERM> signal to that process (e.g. with the C<kill> command); the
parent process stops all of its child processes before
exiting. Default: C</var/run/trackrdr.pid>

=item B<-s status_interval>

Config variable B<monitor.interval>. The minimum number of seconds
between status output to the log, reporting internal statistics (such
as completed records read, currently open records, etc.). Default: 30

=item B<-o processor_logfile>

Config variable B<processor.log>. Log file to contain the contents of
all POST requests to the processor, for debugging purposes. By default
no processor log file is written.

=item B<-b [stdbuf_path]:[stdbuf_bufsize]>

Path and buffer size for the C<stdbuf(1)> utility from GNU
coreutils. If this option is specified, then unbuffered output from
C<varnishlog> is piped into C<stdbuf>, and the script reads the output
from C<stdbuf>; this may be necessary if the output buffer from
C<varnishlog> is too small. C<stdbuf_path> specifies the path for
C<stdbuf> (default: C</usr/bin/stdbuf>), and C<stdbuf_bufsize> is
passed to the C<-o> option of C<stdbuf> to specify the buffer size
(default: 160MB). By default, C<stdbuf> is not used (output is piped
directly from C<varnishlog>).

B<NOTE>: C<stdbuf> requires GNU coreutils >= v7.5.

=item B<--help>

Print usage and exit

=item B<--version>

Print version and exit

=back

=head1 RETURN CODES

Standard return codes for Solaris SMF services are used:

=over

=item 0 (C<SMF_EXIT_OK>)

Normal operation

=item 95 (C<SMF_EXIT_ERR_FATAL>)

Fatal error (for example, fork failed)

=item 96 (C<SMF_EXIT_ERR_CONFIG>)

Configuration error (for example, C<varnishlog> did not start, log or
pid files cannot be opened)

=back

=head1 COPYRIGHT

 Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 Copyright (c) 2012 Otto Gmbh & Co KG
 All rights reserved
 Use only with permission
 Authors: Nils Goroll <nils.goroll@uplex.de>
          Geoffrey Simmons <geoffrey.simmons@uplex.de>

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=cut

use strict;
use warnings;
use threads;
use threads::shared;
use LWP::UserAgent;
use LWP::ConnCache;
use HTTP::Request;
use HTTP::Status qw(RC_NO_CONTENT RC_INTERNAL_SERVER_ERROR);
use Net::STOMP::Client;
use Net::STOMP::Client::Error;
use POSIX qw(setsid);
use FileHandle;
use Getopt::Std;
use Pod::Usage;

$Getopt::Std::STANDARD_HELP_VERSION = 1;
$main::VERSION = "0.5.6";

sub HELP_MESSAGE {
    pod2usage(-exit => 0, -verbose => 1);
}

# be prepared to start with SMF
use constant {
    SMF_EXIT_OK =>          0,
    
    SMF_EXIT_ERR_FATAL =>   95,
    SMF_EXIT_ERR_CONFIG =>  96,
    SMF_EXIT_ERR_NOSMF =>   99,
    SMF_EXIT_ERR_PERM =>    100,
};

use constant (defaultConfig => '/etc/trackrdr.conf');

my %config = (
    'transport'		=>	'',
    'mq.url'		=>	'stomp://127.0.0.1:61613',
    'http.url'		=>	'http://localhost/ts-processor/httpProcess',
    'queue'		=>	'lhotse/tracking/rdr2proc',
    'restarts'		=>	0,
    'debug'		=>	0,
    'log'		=>	'',
    'varnishlog.dump'	=>	'',
    'processor.log'	=>	'',
    'varnish.name'	=>	'',
    'varnish.prefix'	=>	'/var/opt/varnish',
    'pid.file'		=>	'/var/run/trackrdr.pid',
    'monitor.interval'	=>	30,
    'stdbuf.path'	=>	'',
    'stdbuf.bufsize'	=>	'160M',
    );

sub readConfig {
    my ($file, $config) = @_;

    if (! -r $file) {
        $! = SMF_EXIT_ERR_CONFIG;
        die "$file not readable\n";
    }
    my $fh = new FileHandle $file;
    if (! defined $fh) {
        my $err = $!;
        $! = SMF_EXIT_ERR_CONFIG;
        die "Cannot open $file: $err\n";
    }

    while (<$fh>) {
        chomp;
        s/#.*$//;
        s/^\s*//;
        s/\s*$//;
        next unless $_;
        my @kv = split /\s*=\s*/;
        if ($#kv != 1) {
            $! = SMF_EXIT_ERR_CONFIG;
            die "Cannot parse $file line $.: $_\n";
        }
        if (!defined($config->{$kv[0]})) {
            $! = SMF_EXIT_ERR_CONFIG;
            die "Unknown config param $kv[0] ($file line $.)\n";
        }
        $config->{$kv[0]} = $kv[1];
    }
    $fh->close();
}

if (-e defaultConfig) {
    readConfig(defaultConfig, \%config);
}

my %opts;
getopts("dn:v:r:u:f:l:p:s:o:c:m:t:q:b:z:", \%opts);

if ($opts{c}) {
    if (!-e $opts{c}) {
        $! = SMF_EXIT_ERR_CONFIG;
        die "$opts{c} not found\n";
    }
    readConfig($opts{c}, \%config);
}

$config{log}			= $opts{l} if $opts{l};
$config{'varnishlog.dump'}	= $opts{f} if $opts{f};
$config{'processor.log'}	= $opts{o} if $opts{o};
$config{'varnish.prefix'}	= $opts{v} if $opts{v};
$config{'varnish.name'}		= $opts{n} if $opts{n};
$config{'http.url'}		= $opts{u} if $opts{u};
$config{'pid.file'}		= $opts{p} if $opts{p};
$config{'monitor.interval'}	= $opts{s} if $opts{s};
$config{'mq.url'} 		= $opts{m} if $opts{m};
$config{'transport'} 		= $opts{t} if $opts{t};
$config{'queue'}		= $opts{q} if $opts{q};

if (!$config{transport} or $config{transport} !~ /^(mq|http)$/i) {
    $! = SMF_EXIT_ERR_CONFIG;
    die "transport/-t must either MQ or HTTP\n";
}

unless ($config{lc($config{transport}).'.url'}) {
    $! = SMF_EXIT_ERR_CONFIG;
    die "No URL configured for transport $config{transport}\n";
}

if ($config{transport} =~ /mq/i and not $config{queue}) {
    $! = SMF_EXIT_ERR_CONFIG;
    die "No queue configured for transport MQ\n";
}    

if ($config{'varnish.name'} and $config{'varnishlog.dump'}) {
    $! = SMF_EXIT_ERR_CONFIG;
    die "Configure either vanish.name/-n or varnishlog.dump/-f\n";
}    

# 0 to run forever
$config{restarts} = $opts{r} if $opts{r};
if ($config{restarts} !~ /^\d+$/) {
    $! = SMF_EXIT_ERR_CONFIG;
    die "restarts/-r must be numeric ($config{restarts})\n";
}

$config{debug} = $opts{d} if $opts{d};
if ($config{debug}) {
    if ($config{debug} !~ /^(true|false|on|off|yes|no|0|1)$/i) {
        $! = SMF_EXIT_ERR_CONFIG;
        die "debug/-d must be boolean ($config{debug})\n";
    }
    $config{debug} = $config{debug} =~ /^(true|on|yes|1)$/i;
}

if ($opts{b}) {
    my ($path, $bufsize) = split /:/, $opts{b};
    if ($path) {
        $config{'stdbuf.path'} = $path;
    }
    else {
        $config{'stdbuf.path'} = '/usr/bin/stdbuf';
    }
    $config{'stdbuf.bufsize'} = $bufsize if $bufsize;
}

my @SHMTAGS = qw(ReqStart VCL_Log ReqEnd);
my $VARNISHLOG_CMD
    = $config{'varnish.prefix'}."/bin/varnishlog -i ".join(',', @SHMTAGS);
$VARNISHLOG_CMD .= ' -n '.$config{'varnish.name'} if $config{'varnish.name'};

if ($config{'stdbuf.path'}) {
    $VARNISHLOG_CMD = $config{'stdbuf.path'}.' -o'.$config{'stdbuf.bufsize'}.
                      " $VARNISHLOG_CMD -u";
}
elsif ($opts{z}) {
    # undocumented option to use mbuffer
    my ($blocksize, $bufsize) = split /:/, $opts{m};
    $blocksize = 8192 unless $blocksize;
    $bufsize = '160M' unless $bufsize;
    $VARNISHLOG_CMD .= " -u | mbuffer -s $blocksize -m $bufsize -q";
}

use constant {
    DEBUG	=> 0,
    NOTICE	=> 1,
    WARN	=> 2,
    ERROR	=> 3,
    FATAL	=> 4,
};

my @logtag = ("DEBUG", "NOTICE", "WARN", "ERROR", "FATAL");
use constant { ctPost => 'application/x-www-form-urlencoded' };

my $PIDFH = new FileHandle ">".$config{'pid.file'};
unless (defined $PIDFH) {
    my $err = $!;
    $! = SMF_EXIT_ERR_CONFIG;
    die "Cannot open pidfile ".$config{'pid.file'}.": $err\n";
}

my $LOGFH;
if ($config{log}) {
    $LOGFH = new FileHandle ">>$config{log}";
    unless (defined $LOGFH) {
        my $err = $!;
        $! = SMF_EXIT_ERR_CONFIG;
        die "Cannot open $config{log}: $err\n";
    }
}
else {
    $LOGFH = *STDOUT;
}

my $PROCLOGFH;
if ($config{'processor.log'}) {
    $PROCLOGFH = new FileHandle ">".$config{'processor.log'};
    unless (defined $PROCLOGFH) {
        my $err = $!;
        $! = SMF_EXIT_ERR_CONFIG;
        die "Cannot open ".$config{'processor.log'}.": $err\n";
    }
    $PROCLOGFH->autoflush();
}

sub logg {
    my ($level, @args) = @_;

    return if ($level == DEBUG and !$config{debug});
    print $LOGFH "[", scalar(localtime), "] $logtag[$level]: @args\n";
}

sub logflush {
    if ($LOGFH) {
        $LOGFH->flush();
    }
    else {
        STDOUT->flush();
        STDERR->flush();
    }
    if ($PROCLOGFH) {
	$PROCLOGFH->flush();
    }
}

$SIG{__WARN__} = sub { logg (WARN,  @_); };
$SIG{__DIE__}  = sub { logg (FATAL, @_); die "@_\n"; };

if ($config{debug}) {
    for (keys(%config)) {
        logg(DEBUG, "$_ = $config{$_}");
    }
}

our %pids;
our $parent_pid;
our $initial_pid = $$;
our $reopen;
our $term;

our $records :shared;
our $quit :shared;
our $open :shared;
our $dubious :shared;

sub statusThread {
    my ($interval) = @_;

    logg(NOTICE, "Monitor thread starting: tid =", threads->tid());
    while(!$quit) {
	sleep($interval);
	logg(NOTICE, "$records records submitted, $open open, ",
             "$dubious dubious");
	logflush();
    }
    logg(NOTICE, "Monitor thread exiting");
}

sub prepHTTP {
    my $connect = shift;

    $connect->{ua} = new LWP::UserAgent(
        agent		=> "Track Reader Prototype $main::VERSION",
        conn_cache	=> LWP::ConnCache->new(),
        );
}

sub submitHTTP {
    my ($connect, $data) = @_;

    my $ua = $connect->{ua};
    my $req = HTTP::Request->new(POST => $connect->{url},
                                 ['Content_Type'	=> ctPost,
                                  'Content_Length'	=> length($data),
                                 ],
                                 $data);
    logg(DEBUG, "Prepared request: ", $req->as_string);
    my $resp = $ua->request($req);
    if ($resp->code != RC_NO_CONTENT) {
        logg(ERROR, "Processor error: ", $resp->status_line());
    }
    if ($PROCLOGFH) {
        print $PROCLOGFH '[', scalar(localtime), "] $data ", $resp->code, "\n";
    }
}

sub prepMQ {
    my $connect = shift;

    $connect->{mq} = Net::STOMP::Client->new(uri => $config{'mq.url'});
    no warnings 'once';
    $Net::Stomp::Client::Error::Die = 0;
    unless (defined $connect->{mq}->connect()) {
        die "Cannot connect to ", $config{'mq.url'},
            ": $Net::STOMP::Client::Error::Message\n";
    }
    logg(NOTICE, "Successfully connected to ", $config{'mq.url'});
}

sub submitMQ {
    my ($connect, $data) = @_;

    my $mq = $connect->{mq};
    my $status = $mq->send(destination => $connect->{queue}, body => $data);
    unless (defined $status) {
        logg(ERROR, "Cannot send message to queue ", $connect->{queue},
             ": $Net::STOMP::Client::Error::Message\n");
    }
    if ($PROCLOGFH) {
        print $PROCLOGFH '[', scalar(localtime), "] $data ",
              defined $status ? "success" : "FAIL", "\n";
    }
}

our %connect;
if ($config{transport} =~ /mq/i) {
    $connect{url}	= $config{'mq.url'};
    $connect{queue}	= $config{'queue'};
    $connect{prep}	= \&prepMQ;
    $connect{submit}	= \&submitMQ;
}
else {
    $connect{url}	= $config{'http.url'};
    $connect{prep}	= \&prepHTTP;
    $connect{submit}	= \&submitHTTP;
}
    
sub fork_varnishlog {
    my $f = fork;
    if (! defined($f)) {
	my $err = $!;
	$! = SMF_EXIT_ERR_FATAL;
	die ("fork failed: $err");
    } elsif ($f == 0) {
	child_handlers();
	exit(run_varnishlog());
    } else {
	$pids{$f} = 1;
    }
}

sub run_varnishlog {

    $records = 0;

    # Prepare MQ or HTTP transport
    &{$connect{prep}}(\%connect);

    while (1) {
	my $log;
	if ($config{'varnishlog.dump'}) {
	    logg(DEBUG, "logfile=".$config{'varnishlog.dump'});
	    unless (open($log, $config{'varnishlog.dump'})) {
                my $err = $!;
                $! = SMF_EXIT_ERR_CONFIG;
                die ("open ".$config{'varnishlog.dump'}." failed: $err\n");
            }
            logg(NOTICE, "Starting to read ".$config{'varnishlog.dump'});
	}
	else {
	    logg(DEBUG, "varnishlog=$VARNISHLOG_CMD");
	    unless (open($log, '-|', $VARNISHLOG_CMD)) {
	        my $err = $!;
	        $! = SMF_EXIT_ERR_CONFIG;
	        die ("$VARNISHLOG_CMD FAILED: $err\n");
	    }
            logg(NOTICE, "Starting read pipe from $VARNISHLOG_CMD");
	}

        my (%record, %dubious);
	$open = 0;
	$dubious = 0;
        $quit = 0;
	my $monitor = threads->create(\&statusThread,
                                      $config{'monitor.interval'});
	unless (defined $monitor) {
	    logg(ERROR, "Monitor thread failed to start");
	}
	while(<$log>) {
            chomp;
	    next unless $_;
	    my ($tid, $tag, $cb, @in) = split;

            logg(DEBUG, "tid=$tid tag=$tag");

            if ($tag eq 'ReqStart') {
                my $xid = $in[-1];
                logg(DEBUG, "XID=$xid");
		if ($dubious{tid}{$tid}) {
		    delete $dubious{tid}{$tid};
		    warn "Dubious tid $tid undubious at $tag\n";
		}
		if ($dubious{xid}{$xid}) {
		    delete $dubious{xid}{$xid};
		    warn "Dubious xid $xid undubious at $tag\n";
		}
                if ($record{$xid}) {
                    warn "$tag: xid $xid already seen [$_]\n";
                    $dubious{xid}{$xid} = 1;
                    if ($record{$xid}{tid} and $tid != $record{$xid}{tid}) {
                        warn "$tag: tid mismatch, was $record{$xid}{tid}\n";
                        $dubious{tid}{$tid} = 1;
                    }
                }
                $record{$xid}{tid} = $tid;
                push @{$record{xid}{data}}, "XID=$xid";
            }
            elsif ($tag eq 'VCL_Log') {
                next unless $in[0] eq 'track';

                my $xid = $in[1];
                logg(DEBUG, "XID=$xid data=$in[-1]");
                if (!$record{$xid}) {
                    warn "$tag: unexpected xid $xid [$_]\n";
                    $dubious{xid}{$xid} = 1;
                }
                elsif (!$record{$xid}{tid}) {
                    warn "$tag: unknown tid $tid [$_]\n";
                    $record{$xid}{tid} = $tid;
                    $dubious{tid}{$tid} = 1;
                }
		elsif ($tid != $record{$xid}{tid}) {
		    warn "$tag: tid mismatch, was $tid [$_]\n";
		    $dubious{tid}{$tid} = 1;
		}
                push @{$record{$xid}{data}}, $in[-1];
            }
            elsif ($tag eq 'ReqEnd') {
                my $xid = $in[0];
                logg(DEBUG, "XID=$xid");

                if (!$record{$xid}) {
                    warn "$tag: Unknown XID $xid [$_]\n";
                    $dubious{xid}{$xid} = 1;
                    $dubious{tid}{$tid} = 1;
                    next;
                }

		if ($dubious{tid}{$tid}) {
		    delete $dubious{tid}{$tid};
		    warn "Dubious tid $tid undubious at $tag\n";
		}
		if ($dubious{xid}{$xid}) {
		    delete $dubious{xid}{$xid};
		    warn "Dubious xid $xid undubious at $tag\n";
		}

                if (!$record{$xid}{tid}) {
                    warn "$tag: Unknown tid $tid [$_]\n";
                    $dubious{tid}{$tid} = 1;
                }
                elsif ($tid != $record{$xid}{tid}) {
                    warn "$tag: tid mismatch, was $record{$xid}{tid} [$_]\n";
                    $dubious{tid}{$tid} = 1;
                }

                if ($record{$xid}{data}) {
                    my $data = join('&', @{$record{$xid}{data}});
                    logg(DEBUG, "DATA=[$data]");
                    $records++;
                    logg(DEBUG, "$records complete records found");
                    &{$connect{submit}}(\%connect, $data);
                }
                delete $record{$xid};
            }
	    $open = scalar(keys %record);
	    $dubious = scalar(keys %{$dubious{tid}})
                       + scalar(keys %{$dubious{xid}});
	}
	close($log);
	if ($config{'varnishlog.dump'}) {
	    kill('TERM', $parent_pid);
            logg(NOTICE, "exiting");
	    logflush();
	    exit(SMF_EXIT_OK);
        }
	$quit = 1;
	$monitor->join();
	logg(NOTICE, "varnishlog restart");
    }
    # should never get here
    exit(SMF_EXIT_ERR_FATAL);
}

## child forwards SIGPIPE to parent
## parent sends TERM to all children and exists

sub child_handlers {
    $SIG{'HUP'} = 'IGNORE';
    $SIG{'INT'} = 'IGNORE';
    $SIG{'QUIT'} = 'IGNORE';

    $SIG{'PIPE'} = \&child_sigpipe;
    $SIG{'TERM'} = 'DEFAULT';
}

sub child_sigpipe {
    kill PIPE => $parent_pid;
}

sub parent_handlers {
    $SIG{'HUP'} = 'IGNORE';
    $SIG{'INT'} = 'IGNORE';
    $SIG{'QUIT'} = 'IGNORE';

    $SIG{'PIPE'} = \&parent_pipe;
    $SIG{'TERM'} = \&parent_term;
}

sub parent_term {
    &parent_pipe;
    $term = 1;
}

# we can't write to our output pipe, so we kill all children.
# the 2nd level wait-loop will terminate and the top level
# while loop will terminate too

sub parent_pipe {
    $reopen = 1;
    my @p = (keys %pids);
    if (scalar(@p)) {
	kill('TERM', @p);
    }
}

sub initial_handlers {
    $SIG{'USR1'} = \&initial_usr1;
}

sub initial_usr1 {
    # the initial process exits with OK when the parent/daemon is ready
    exit(SMF_EXIT_OK);
}

sub parent_init {
    unless (setsid) {
	my $err = $!;
	$! = SMF_EXIT_ERR_FATAL;
	die ("setsid failed: $err\n");
    }

    initial_handlers();

    my $f = fork;
    if (! defined($f)) {
	my $err = $!;
	$! = SMF_EXIT_ERR_FATAL;
	die ("fork failed: $err\n");
    } elsif ($f == 0) {
	$parent_pid = $$;
        print $PIDFH $$;
        $PIDFH->close();
        logg(NOTICE, "Parent (watchdog) process starting (v$main::VERSION)");
	return;
    } else {
	$parent_pid = $f;
	# in the initial process
	#
	# wait for a child error - if any
	# when receiving a signal, we'll exit OK
	wait;
	exit($?);
    }
}

{
    my $initial_notified = 0;

    sub parent_ready {
	return if ($initial_notified);
	$initial_notified = 1;
	kill 'USR1' => $initial_pid;
    }
}

parent_init();

$term = 0;
while(! $term) {
    $reopen = 0;

    parent_handlers();
    fork_varnishlog();

    parent_ready();

    my $restarts = 0;

    while ((my $pid = wait) != -1) {

	next unless (exists($pids{$pid}));
	undef $pids{$pid};
	next if ($reopen || $term);

	logg(NOTICE, "varnishlog perl reader for pid $pid died: ",
             "restart ", ++$restarts);

	if ($config{restarts} &&
	    ($restarts > $config{restarts})) {
	    logg(FATAL, "too many restarts: $restarts");
	    parent_term;
	}

	fork_varnishlog();
    }

    # make sure all children have terminated
    my @p = (keys %pids);
    if (scalar(@p)) {
	kill('TERM', @p);
    }
    while (wait != -1) {};
}
logg(NOTICE, "exiting");
logflush();
exit (SMF_EXIT_OK);
