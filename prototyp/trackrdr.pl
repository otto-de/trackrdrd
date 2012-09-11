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

 $ trackrdr.pl [[-n varnish_logfile] | [-f varnishlog_outputfile]]
               [-v varnish_prefix] [-r max_restarts] [-u processor_url] [-d]
               [-l logfile] [-p pidfile] [-s status_interval]
               [--help] [--version]

=head1 DESCRIPTION

C<trackrdr.pl> starts an instance of C<varnishlog>, parses its
output for tags relevant to tracking, and sends complete data records
to the processor via HTTP.

=head1 OPTIONS

No command line options are required, defaults are described in the
following.

=over

=item B<-n varnish_logfile>

The "varnish name" indicating the mmap'd log file used by C<varnishd>
and C<varnishlog>, used for the C<-n> option to start
C<varnishlog>. By default, C<varnishlog> is started without an C<-n>
option (so the default for C<varnishlog> holds).

=item B<-f varnishlog_outputfile>

Path of a file created by redirecting standard output of
C<varnishlog>, useful for debugging purposes. The options B<-n> and
B<-f> are mutually exclusive. By default, the default choice for B<-n>
is assumed (read the SHM log at the default location for
C<varnishlog>).

=item B<-v varnish_prefix>

Installation directory for varnish, default: C</var/opt/varnish>

=item B<-r max_restarts>

Maximum number of restarts for the child process, or 0 for unlimited,
default 0

=item B<-d>

Switches on debug mode, off by default

=item B<-u processor_url>

URL of the processor application, to which data records are submitted.
Default: C<http://localhost/ts-processor/httpProcess>

=item B<-l logfile>

Log file for status, warning, debug and error messages. By default,
status and debug messages are written to C<STDOUT>, and warnings and
error messages are written to C<STDERR>.

=item B<-p pidfile>

File in which the process ID of the parent process is stored. To stop
the script, it suffices to send a C<TERM> signal to that process
(e.g. with the C<kill> command); the parent process stops all of its
child processes before exiting. Default: C</var/run/trackrdr.pid>

=item B<-s status_interval>

The minimum number of seconds between status output to the log,
reporting interbal statistics (such as completed records read,
currently open records, etc.).

=item B<-o processor_logfile>

Log file to contain the contents of all POST requests to the processor, for debugging purposes. By default no processor log file is written.

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
use HTTP::Status qw(RC_NO_CONTENT RC_INTERNAL_SERVER_ERROR);
use POSIX qw(setsid);
use FileHandle;
use Getopt::Std;
use Pod::Usage;

$Getopt::Std::STANDARD_HELP_VERSION = 1;
$main::VERSION = "0.4";

sub HELP_MESSAGE {
    pod2usage(-exit => 0, -verbose => 1);
}

my %opts;
getopts("dn:v:r:u:f:l:p:s:o:", \%opts);

# 0 to run forever
my $MAX_RESTARTS = $opts{r} || 0;
my $DEBUG = $opts{d} || 0;
my $LOGFILE = $opts{l};
my $VLOGFILE = $opts{f};
my $PROCLOGFILE = $opts{o};

my @SHMTAGS = qw(ReqStart VCL_Log ReqEnd);
my $VARNISH_PRE = $opts{v} || '/var/opt/varnish';
# my $VARNISHLOG_CMD = "/usr/bin/stdbuf -o100M $VARNISH_PRE/bin/varnishlog -i ".join(',', @SHMTAGS);
my $VARNISHLOG_CMD = "$VARNISH_PRE/bin/varnishlog -i ".join(',', @SHMTAGS);
$VARNISHLOG_CMD = "$VARNISHLOG_CMD -n $opts{n}" if $opts{n};

my $PROC_URL = $opts{u} || 'http://localhost/ts-processor/httpProcess';

my $PIDFILE = $opts{p} || '/var/run/trackrdr.pid';

my $SLEEP = $opts{s} || 30;

# be prepared to start with SMF
use constant {
    SMF_EXIT_OK =>          0,
    
    SMF_EXIT_ERR_FATAL =>   95,
    SMF_EXIT_ERR_CONFIG =>  96,
    SMF_EXIT_ERR_NOSMF =>   99,
    SMF_EXIT_ERR_PERM =>    100,
};

use constant {
    DEBUG	=> 0,
    NOTICE	=> 1,
    WARN	=> 2,
    FATAL	=> 3,
};

my @logtag = ("DEBUG", "NOTICE", "WARN", "FATAL");

my $PIDFH = new FileHandle "> $PIDFILE";
unless (defined $PIDFH) {
    my $err = $!;
    $! = SMF_EXIT_ERR_CONFIG;
    die "Cannot open pidfile $PIDFILE: $err\n";
}

my $LOGFH;
if ($LOGFILE) {
    $LOGFH = new FileHandle ">$LOGFILE";
    unless (defined $LOGFH) {
        my $err = $!;
        $! = SMF_EXIT_ERR_CONFIG;
        die "Cannot open $LOGFILE: $err\n";
    }
}
else {
    $LOGFH = *STDOUT;
}

my $PROCLOGFH;
if ($PROCLOGFILE) {
    $PROCLOGFH = new FileHandle ">$PROCLOGFILE";
    unless (defined $PROCLOGFH) {
        my $err = $!;
        $! = SMF_EXIT_ERR_CONFIG;
        die "Cannot open $PROCLOGFILE: $err\n";
    }
    $PROCLOGFH->autoflush();
}

sub logg {
    my ($level, @args) = @_;

    return if ($level == DEBUG and !$DEBUG);
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

    logg(NOTICE, "Monitoring thread starting: tid =", threads->tid());
    while(!$quit) {
	sleep($interval);
	logg(NOTICE, "$records records submitted,",
	     "$open records open, $dubious records dubious");
	logflush();
    }
    logg(NOTICE, "Monitor thread exiting");
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

    my $ua = new LWP::UserAgent(
        agent		=> "Track Reader Prototype $main::VERSION",
        conn_cache	=> LWP::ConnCache->new(),
        );

    $records = 0;
    $quit = 0;

    while (1) {
        logg(DEBUG, "varnishlog=$VARNISHLOG_CMD");
	my $log;
	if ($VLOGFILE) {
	    logg(DEBUG, "logfile=$VLOGFILE");
	    unless (open($log, $VLOGFILE)) {
                my $err = $!;
                $! = SMF_EXIT_ERR_CONFIG;
                die ("open $VLOGFILE failed: $err\n");
            }
            logg(NOTICE, "Starting to read $VLOGFILE");
	}
	else {
	    logg(DEBUG, "varnishlog=$VARNISHLOG_CMD");
	    unless (open($log, '-|', $VARNISHLOG_CMD)) {
	        my $err = $!;
	        $! = SMF_EXIT_ERR_CONFIG;
	        die ("runnning varnishlog failed: $err\n");
	    }
            logg(NOTICE, "Starting read pipe from $VARNISHLOG_CMD");
	}

        my (%record, %dubious_tid);
	$open = 0;
	$dubious = 0;
        #my $laststatus = time();
	my $monitor = threads->create(\&statusThread, $SLEEP, $records, $open,
				      $dubious, $quit);
	unless (defined $monitor) {
	    logg(WARN, "Monitor thread failed to start");
	}
	while(<$log>) {
            chomp;
	    next unless $_;
	    my ($tid, $tag, $cb, @in) = split;

            logg(DEBUG, "tid=$tid tag=$tag");

            if ($tag eq 'ReqStart') {
                $record{$tid}{xid} = $in[-1];
                logg(DEBUG, "XID=$record{$tid}{xid}");
		if ($dubious_tid{$tid}) {
		    delete $dubious_tid{$tid};
		    warn "Dubious tid $tid undubious at ReqStart";
		}
            }
            elsif ($tag eq 'VCL_Log') {
                next unless $in[0] eq 'track';
		if (!$record{$tid}) {
		    warn "$tag: unexpected tid $tid [$_]\n";
		    $record{$tid}{xid} = $in[1];
		    $dubious_tid{$tid} = 1;
		}
		elsif (!$record{$tid}{xid}) {
		    warn "$tag: no xid known for tid $tid [$_]\n";
		}
                elsif ($in[1] ne $record{$tid}{xid}) {
                    warn "$tag: xid mismatch, expected $record{$tid}{xid}, ",
                         "got $in[1] [$_]\n";
                }
                else {
                    push @{$record{$tid}{data}}, $in[-1];
                    logg(DEBUG, "XID=$record{$tid}{xid} data=$in[-1]");
                }
            }
            elsif ($tag eq 'ReqEnd') {
                if ($record{$tid}
                    && $record{$tid}{xid}
                    && $record{$tid}{xid} eq $in[0]) {
                    if ($record{$tid}{data}) {
                        my $data = join('&', @{$record{$tid}{data}});
			$records++;
			logg(DEBUG, "$records complete records found");
			if ($PROCLOGFH) {
			    print $PROCLOGFH
				'[', scalar(localtime), "] $data\n";
			}
                        my $resp = $ua->post($PROC_URL, Content => $data);
                        if ($resp->code != RC_NO_CONTENT) {
                            logg(WARN, "Processor error: ",
                                 $resp->status_line());
                        }
                        logg(DEBUG,
                             'DATA: ', join('&', @{$record{$tid}{data}}));
                    }
                    delete $record{$tid};
		    if ($dubious_tid{$tid}) {
			delete $dubious_tid{$tid};
			logg(WARN, "Dubious tid $tid undubious at ReqEnd\n");
		    }
                }
                else {
		    if (!$record{$tid}) {
			logg(WARN, "$tag: unexpected tid $tid [$_]\n");
		    }
		    elsif (!$record{$tid}{xid}) {
			logg(WARN,
                             "$tag: no req XID known for tid $tid [$_]\n");
		    }
                    elsif ($record{$tid}{xid} ne $in[0]) {
                        logg(WARN, "$tag: req XID mismatch $in[0] [$_]\n");
                    }
                    else {
                        logg(WARN, "$tag: record incomplete: [$_]\n");
                    }
                    foreach my $key (keys %{$record{$tid}}) {
                        logg(WARN, "\t$key=$record{$tid}{$key}\n");
                    }
                }
            }
	    $open = scalar(keys %record);
	    $dubious = scalar(keys %dubious_tid);
#            if (time() >= $laststatus + $SLEEP) {
#                logg(NOTICE, "$records records submitted, ",
#                     scalar(keys %record), " records open, ",
#                     scalar(keys %dubious_tid), " records dubious");
#                logflush();
#                $laststatus = time();
#            }
	}
	close($log);
	if ($VLOGFILE) {
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
        logg(NOTICE, "Parent (watchdog) process starting");
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

	if ($MAX_RESTARTS &&
	    ($restarts > $MAX_RESTARTS)) {
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
