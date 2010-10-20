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

use constant DEFAULT_ADDR	=> '192.168.3.100';
use constant MY_ECHO_PORT	=> 5000;
my ($byte_out, $byte_in) = (0,0);
$/ = "\n";
$| = 1;

print "\nUsage: client.pl tcp|udp [ip] [port]\n\n";

my $quit = 0;
$SIG{INT}  = sub { $quit++ };

my $tcp = shift;
my $address = shift || DEFAULT_ADDR;
my $port = shift || MY_ECHO_PORT;

print "\nTry to connect to a $tcp server of $address:$port...\n";

my $sock = IO::Socket::INET->new(PeerAddr => $address,
                                 PeerPort => $port,
                                 Proto  => $tcp)
            or die "Can't connect: $!\n";

while (my $msg_out = <>)
{
    last if $msg_out =~ /^\.$/;
    print "out: $msg_out\n";
    $sock->send($msg_out);

    my $msg_in;
    $sock->recv($msg_in, 10000);
    print "in: $msg_in\n";
}

close $sock;



