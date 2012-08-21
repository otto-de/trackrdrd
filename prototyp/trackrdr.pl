#!/usr/bin/perl

##
## Prototyp for the tracking reader
##
## opens varnishlog and parses the relevant tags
## sends complete data records to the processor
## XXX: currently writes data records to stdout
##
## For copyright and licensing notices, see the pod section COPYRIGHT
## below
##

=head1 NAME

trackrdr.pl - read tracking data from the Varnish SHM-log and send
data sets to the processor
XXX: Currently writes data sets to stdout

=head1 SYNOPSIS

 $ trackrdr.pl [-n logfile] [-p varnish_prefix] [-r max_restarts] [-d]
               [--help] [--version]

=head1 DESCRIPTION

C<varnish_2_splunk.pl> starts an instance of C<varnishlog>, parses its
output for tags relevant to tracking, and sends complete data records
to the processor via HTTP. XXX: Currently, data records are written to
stdout.

=head1 OPTIONS

No command line options are required, defaults are described in the
following.

=over

=item -n logfile

The "varnish name" indicating the mmap'd log file used by C<varnishd>
and C<varnishlog>, used for the C<-n> option to start
C<varnishlog>. By default, C<varnishlog> is started without an C<-n>
option (so the default for C<varnishlog> holds).

=item -p varnish_prefix

Installation directory for varnish, default: C</var/opt/varnish>

=item -r max_restarts

Maximum number of restarts for the child process, or 0 for unlimited,
default 0

=item -d

Switches on debug mode, off by default

=item --help

Print usage and exit

=item --version

Print version and exit

=back

=head1 RETURN CODES

Standard return codes for Solaris SMF services are used:

=over

=item 0 (C<SMF_EXIT_OK>)

Normal operation

=item 95 (C<SMF_EXIT_ERR_FATAL>)

Fatal error (for example, fork failed or C<varnishlog> did not start)

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
use POSIX qw(setsid);
use Getopt::Std;
use Pod::Usage;

$Getopt::Std::STANDARD_HELP_VERSION = 1;
$main::VERSION = "0.1-SNAPSHOT";

sub HELP_MESSAGE {
    pod2usage(-exit => 0, -verbose => 1);
}

my %opts;
getopts("dn:p:r:", \%opts);

# 0 to run forever
my $MAX_RESTARTS = $opts{r} || 0;
my $DEBUG = $opts{d} || 0;

my @SHMTAGS = qw(ReqStart VCL_Log ReqEnd);
my $VARNISH_PRE = $opts{p} || '/var/opt/varnish';
my $VARNISHLOG_CMD = "$VARNISH_PRE/bin/varnishlog -u -i ".join(',', @SHMTAGS);
$VARNISHLOG_CMD = "$VARNISHLOG_CMD -n $opts{n}" if $opts{n};

# be prepared to start with SMF

use constant {
    SMF_EXIT_OK =>          0,
    
    SMF_EXIT_ERR_FATAL =>   95,
    SMF_EXIT_ERR_CONFIG =>  96,
    SMF_EXIT_ERR_NOSMF =>   99,
    SMF_EXIT_ERR_PERM =>    100,
};

our %pids;
our $parent_pid;
our $initial_pid = $$;
our $reopen;
our $term;

sub fork_varnishlog {
    my $f = fork;
    if (! defined($f)) {
	my $err = $!;
	$! = SMF_EXIT_ERR_FATAL;
	die ("fork failed: $err\n");
    } elsif ($f == 0) {
	child_handlers();
	exit(run_varnishlog());
    } else {
	$pids{$f} = 1;
    }
}

sub run_varnishlog {

    while (1) {
        print "varnishlog=$VARNISHLOG_CMD\n" if $DEBUG;
	my $log;
	unless (open($log, '-|', $VARNISHLOG_CMD)) {
	    my $err = $!;
	    $! = SMF_EXIT_ERR_FATAL;
	    die ("runnning varnishlog failed: $err\n");
	}

        my (%record);
	while(<$log>) {
            chomp;
	    my ($tid, $tag, $cb, @in) = split;

            print "tid=$tid tag=$tag\n" if $DEBUG;

            if ($tag eq 'ReqStart') {
                $record{$tid}{xid} = $in[-1];
                print "XID=$record{$tid}{xid}\n" if $DEBUG;
                next;
            }
            if ($tag eq 'VCL_Log') {
                next unless $in[0] eq 'track';
                if ($in[1] ne $record{$tid}{xid}) {
                    warn "$tid: xid mismatch, expected $record{$tid}{xid}, ",
                         "got $in[1]\n";
                }
                else {
                    push @{$record{$tid}{data}}, $in[-1];
                    print "XID=$record{$tid}{xid} data=$in[-1]\n"
                        if $DEBUG;
                }
                next;
            }
            if ($tag eq 'ReqEnd') {
                if ($record{$tid}
                    && $record{$tid}{xid}
                    && $record{$tid}{xid} eq $in[0]) {
                    if ($record{$tid}{data}) {
                        # XXX: HTTP call goes here
                        print 'DATA: ', join('&', @{$record{$tid}{data}}), "\n";
                    }
                    delete $record{$tid};
                }
                else {
                    if ($record{$tid}{xid} ne $in[0]) {
                        warn "$tag: req XID mismatch $in[1]\n";
                    }
                    else {
                        warn "$tag: record incomplete:\n";
                    }
                    foreach my $key (keys %{$record{$tid}}) {
                        print "\t$key=$record{$tid}{$key}\n";
                    }
                }
            }
	}
	close($log);
	print "NOTICE: varnishlog restart at ", localtime(time), "\n";
    }
    # should never get here
    SMF_EXIT_ERR_FATAL;
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

	print "NOTICE: varnishlog perl reader for pid $pid died\n",
              "\trestart ".++$restarts." at ".localtime(time)."\n";

	if ($MAX_RESTARTS &&
	    ($restarts > $MAX_RESTARTS)) {
	    warn ("too many restarts: $restarts\n");
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
exit (SMF_EXIT_OK);
