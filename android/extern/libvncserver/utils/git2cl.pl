#!/usr/bin/perl

# Copyright (C) 2007, 2008 Simon Josefsson <simon@josefsson.org>
# Copyright (C) 2007 Luis Mondesi <lemsx1@gmail.com>
# * calls git directly. To use it just: 
#   cd ~/Project/my_git_repo; git2cl > ChangeLog
# * implements strptime()
# * fixes bugs in $comment parsing
#   - copy input before we remove leading spaces
#   - skip "merge branch" statements as they don't
#     have information about files (i.e. we never
#     go into $state 2)
#   - behaves like a pipe/filter if input is given from the CLI
#     else it calls git log by itself
#
# The functions mywrap, last_line_len, wrap_log_entry are derived from
# the cvs2cl tool, see <http://www.red-bean.com/cvs2cl/>:
# Copyright (C) 2001,2002,2003,2004 Martyn J. Pearce <fluffy@cpan.org>
# Copyright (C) 1999 Karl Fogel <kfogel@red-bean.com>
#
# git2cl is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# git2cl is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with git2cl; see the file COPYING.  If not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

use strict;
use POSIX qw(strftime);
use Text::Wrap qw(wrap);
use FileHandle;

use constant EMPTY_LOG_MESSAGE => '*** empty log message ***';

# this is a helper hash for stptime.
# Assumes you are calling 'git log ...' with LC_ALL=C
my %month = (
    'Jan'=>0,
    'Feb'=>1,
    'Mar'=>2,
    'Apr'=>3,
    'May'=>4,
    'Jun'=>5,
    'Jul'=>6,
    'Aug'=>7,
    'Sep'=>8,
    'Oct'=>9,
    'Nov'=>10,
    'Dec'=>11,
);

my $fh = new FileHandle;

sub key_ready
{
    my ($rin, $nfd);
    vec($rin, fileno(STDIN), 1) = 1;
    return $nfd = select($rin, undef, undef, 0);
}

sub strptime {
    my $str = shift;
    return undef if not defined $str;

    # we are parsing this format
    # Fri Oct 26 00:42:56 2007 -0400
    # to these fields
    # sec, min, hour, mday, mon, year, wday = -1, yday = -1, isdst = -1
    # Luis Mondesi <lemsx1@gmail.com>
    my @date;
    if ($str =~ /([[:alpha:]]{3})\s+([[:alpha:]]{3})\s+([[:digit:]]{1,2})\s+([[:digit:]]{1,2}):([[:digit:]]{1,2}):([[:digit:]]{1,2})\s+([[:digit:]]{4})/){
        push(@date,$6,$5,$4,$3,$month{$2},($7 - 1900),-1,-1,-1);
    } else {
        die ("Cannot parse date '$str'\n'");
    }
    return @date;
}

sub mywrap {
    my ($indent1, $indent2, @text) = @_;
    # If incoming text looks preformatted, don't get clever
    my $text = Text::Wrap::wrap($indent1, $indent2, @text);
    if ( grep /^\s+/m, @text ) {
	return $text;
    }
    my @lines = split /\n/, $text;
    $indent2 =~ s!^((?: {8})+)!"\t" x (length($1)/8)!e;
    $lines[0] =~ s/^$indent1\s+/$indent1/;
    s/^$indent2\s+/$indent2/
	for @lines[1..$#lines];
    my $newtext = join "\n", @lines;
    $newtext .= "\n"
	if substr($text, -1) eq "\n";
    return $newtext;
}

sub last_line_len {
    my $files_list = shift;
    my @lines = split (/\n/, $files_list);
    my $last_line = pop (@lines);
    return length ($last_line);
}

# A custom wrap function, sensitive to some common constructs used in
# log entries.
sub wrap_log_entry {
    my $text = shift;                  # The text to wrap.
    my $left_pad_str = shift;          # String to pad with on the left.

    # These do NOT take left_pad_str into account:
    my $length_remaining = shift;      # Amount left on current line.
    my $max_line_length  = shift;      # Amount left for a blank line.

    my $wrapped_text = '';             # The accumulating wrapped entry.
    my $user_indent = '';              # Inherited user_indent from prev line.

    my $first_time = 1;                # First iteration of the loop?
    my $suppress_line_start_match = 0; # Set to disable line start checks.

    my @lines = split (/\n/, $text);
    while (@lines)   # Don't use `foreach' here, it won't work.
    {
	my $this_line = shift (@lines);
	chomp $this_line;

	if ($this_line =~ /^(\s+)/) {
	    $user_indent = $1;
	}
	else {
	    $user_indent = '';
	}

	# If it matches any of the line-start regexps, print a newline now...
	if ($suppress_line_start_match)
	{
	    $suppress_line_start_match = 0;
	}
	elsif (($this_line =~ /^(\s*)\*\s+[a-zA-Z0-9]/)
	       || ($this_line =~ /^(\s*)\* [a-zA-Z0-9_\.\/\+-]+/)
	       || ($this_line =~ /^(\s*)\([a-zA-Z0-9_\.\/\+-]+(\)|,\s*)/)
	       || ($this_line =~ /^(\s+)(\S+)/)
	       || ($this_line =~ /^(\s*)- +/)
	       || ($this_line =~ /^()\s*$/)
	       || ($this_line =~ /^(\s*)\*\) +/)
	       || ($this_line =~ /^(\s*)[a-zA-Z0-9](\)|\.|\:) +/))
	{
	    $length_remaining = $max_line_length - (length ($user_indent));
	}

	# Now that any user_indent has been preserved, strip off leading
	# whitespace, so up-folding has no ugly side-effects.
	$this_line =~ s/^\s*//;

	# Accumulate the line, and adjust parameters for next line.
	my $this_len = length ($this_line);
	if ($this_len == 0)
	{
	    # Blank lines should cancel any user_indent level.
	    $user_indent = '';
	    $length_remaining = $max_line_length;
	}
	elsif ($this_len >= $length_remaining) # Line too long, try breaking it.
	{
	    # Walk backwards from the end.  At first acceptable spot, break
	    # a new line.
	    my $idx = $length_remaining - 1;
	    if ($idx < 0) { $idx = 0 };
	    while ($idx > 0)
	    {
		if (substr ($this_line, $idx, 1) =~ /\s/)
		{
		    my $line_now = substr ($this_line, 0, $idx);
		    my $next_line = substr ($this_line, $idx);
		    $this_line = $line_now;

		    # Clean whitespace off the end.
		    chomp $this_line;

		    # The current line is ready to be printed.
		    $this_line .= "\n${left_pad_str}";

		    # Make sure the next line is allowed full room.
		    $length_remaining = $max_line_length - (length ($user_indent));

		    # Strip next_line, but then preserve any user_indent.
		    $next_line =~ s/^\s*//;

		    # Sneak a peek at the user_indent of the upcoming line, so
		    # $next_line (which will now precede it) can inherit that
		    # indent level.  Otherwise, use whatever user_indent level
		    # we currently have, which might be none.
		    my $next_next_line = shift (@lines);
		    if ((defined ($next_next_line)) && ($next_next_line =~ /^(\s+)/)) {
			$next_line = $1 . $next_line if (defined ($1));
			# $length_remaining = $max_line_length - (length ($1));
			$next_next_line =~ s/^\s*//;
		    }
		    else {
			$next_line = $user_indent . $next_line;
		    }
		    if (defined ($next_next_line)) {
			unshift (@lines, $next_next_line);
		    }
		    unshift (@lines, $next_line);

		    # Our new next line might, coincidentally, begin with one of
		    # the line-start regexps, so we temporarily turn off
		    # sensitivity to that until we're past the line.
		    $suppress_line_start_match = 1;

		    last;
		}
		else
		{
		    $idx--;
		}
	    }

	    if ($idx == 0)
	    {
		# We bottomed out because the line is longer than the
		# available space.  But that could be because the space is
		# small, or because the line is longer than even the maximum
		# possible space.  Handle both cases below.

		if ($length_remaining == ($max_line_length - (length ($user_indent))))
		{
		    # The line is simply too long -- there is no hope of ever
		    # breaking it nicely, so just insert it verbatim, with
		    # appropriate padding.
		    $this_line = "\n${left_pad_str}${this_line}";
		}
		else
		{
		    # Can't break it here, but may be able to on the next round...
		    unshift (@lines, $this_line);
		    $length_remaining = $max_line_length - (length ($user_indent));
		    $this_line = "\n${left_pad_str}";
		}
	    }
	}
	else  # $this_len < $length_remaining, so tack on what we can.
	{
	    # Leave a note for the next iteration.
	    $length_remaining = $length_remaining - $this_len;

	    if ($this_line =~ /\.$/)
	    {
		$this_line .= "  ";
		$length_remaining -= 2;
	    }
	    else  # not a sentence end
	    {
		$this_line .= " ";
		$length_remaining -= 1;
	    }
	}

	# Unconditionally indicate that loop has run at least once.
	$first_time = 0;

	$wrapped_text .= "${user_indent}${this_line}";
    }

    # One last bit of padding.
    $wrapped_text .= "\n";

    return $wrapped_text;
}

# main

my @date;
my $author;
my @files;
my $comment;

my $state; # 0-header 1-comment 2-files
my $done = 0;

$state = 0;

# if reading from STDIN, we assume that we are
# getting git log as input
if (key_ready())
{

    #my $dummyfh; # don't care about writing
    #($fh,$dummyfh) = FileHandle::pipe;
    $fh->fdopen(*STDIN, 'r');
}
else
{
    $fh->open("LC_ALL=C git log --pretty --numstat --summary|")
	or die("Cannot execute git log...$!\n");
}

while (my $_l = <$fh>) {
    #print STDERR "debug ($state, " . (@date ? (strftime "%Y-%m-%d", @date) : "") . "): `$_'\n";
    if ($state == 0) {
	if ($_l =~ m,^Author: (.*),) {
	    $author = $1;
	}
	if ($_l =~ m,^Date: (.*),) {
	    @date = strptime($1);
	}
	$state = 1 if ($_l =~ m,^$, and $author and (@date+0>0));
    } elsif ($state == 1) {
        # * modifying our input text is a bad choice
        #   let's make a copy of it first, then we remove spaces 
        # * if we meet a "merge branch" statement, we need to start
        #   over and find a real entry
        # Luis Mondesi <lemsx1@gmail.com>
        my $_s = $_l;
	$_s =~ s/^    //g;
        if ($_s =~ m/^Merge branch|^Merge remote branch/)
        {
            $state=0;
            $author=0;
            next;
        }
	$comment = $comment . $_s;
	$state = 2 if ($_l =~ m,^$,);
    } elsif ($state == 2) {
	if ($_l =~ m,^([0-9]+)\t([0-9]+)\t(.*)$,) {
	    push @files, $3;
	}
	$done = 1 if ($_l =~ m,^$,);
    }

    if ($done) {
	print (strftime "%Y-%m-%d  $author\n\n", @date);

	my $files = join (", ", @files);
	$files = mywrap ("\t", "\t", "* $files"), ": ";

	if (index($comment, EMPTY_LOG_MESSAGE) > -1 ) {
	    $comment = "[no log message]\n";
	}

	my $files_last_line_len = 0;
	$files_last_line_len = last_line_len($files) + 1;
	my $msg = wrap_log_entry($comment, "\t", 69-$files_last_line_len, 69);

	$msg =~ s/[ \t]+\n/\n/g;

	print "$files: $msg\n";

	@date = ();
	$author = "";
	@files = ();
	$comment = "";

	$state = 0;
	$done = 0;
    }
}

if (@date + 0)
{
    print (strftime "%Y-%m-%d  $author\n\n", @date);
    my $msg = wrap_log_entry($comment, "\t", 69, 69);
    $msg =~ s/[ \t]+\n/\n/g;
    print "\t* $msg\n";
}
