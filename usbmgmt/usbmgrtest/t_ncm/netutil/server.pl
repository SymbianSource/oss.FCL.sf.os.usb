#!/usr/bin/perl -w
# Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
# All rights reserved.
# This component and the accompanying materials are made available
# under the terms of "Eclipse Public License v1.0"
# which accompanies this distribution, and is available
# at the URL "http://www.eclipse.org/legal/epl-v10.html".
#
# Initial Contributors:
# Nokia Corporation - initial contribution.
#
# Contributors:
#
# Description:
#
use strict;
use IO::Socket qw(:DEFAULT :crlf);

use constant MY_ECHO_PORT	=> 5000;
$/ = "\n";
$| = 1;
my $tcp = 0;
print "\nUsage: server.pl tcp|udp [port]\n\n";

my ($byte_out, $byte_in) = (0,0);

my $quit = 0;
$SIG{INT}  = sub { $quit++ };

my $tcp = shift eq 'tcp';
my $port = shift || MY_ECHO_PORT;
my $sock;

if ($tcp)
{
    my $sock = IO::Socket::INET->new (
	Listen	=> 20,
	LocalPort => $port,
	Timeout	=> 60*60,
	Reuse	=> 1 )
	or die "Can not create listening socket: $!\n";
    warn "waiting for the incoming connections on port $port...\n";

    while ($tcp and !$quit) {
	next unless my $session = $sock->accept;
	my $peer = gethostbyaddr($session->peeraddr, AF_INET) || $session->peerhost;
	warn "Connection from [$peer, $port]\n";

	while(<$session>) {
	    $byte_in += length($_);
	    chomp;
	    print "in: $_\n";
	    my $msg_out = (scalar reverse $_) . CRLF;
	    print "send back: $msg_out";
	    print $session $msg_out;
	    $byte_out += length($msg_out);
	}
	warn "Connection from [$peer, $port] finished\n";
	close $session;
    }
}

if (not $tcp)
{
    my $sock = IO::Socket::INET->new(
	LocalPort  => $port,
	Type   => SOCK_DGRAM,
	Proto  => 'udp') or die "Can't connect: $!\n";

    warn "waiting for the incoming udp data on port $port...\n";

    while (!$quit) {
	my $msg_in;
	$sock->recv($msg_in, 10000);
	$byte_in += length($msg_in);
	chomp($msg_in);
	print "in: $msg_in\n";
	my $msg_out = (scalar reverse $msg_in) . CRLF;
	print "send back: $msg_out";
	$sock->send($msg_out);
	$byte_out += length($msg_out);
    }
}

print STDERR "out = $byte_out, in = $byte_in\n";
close $sock;

