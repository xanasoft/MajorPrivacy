#pragma once
#include <QString>
#include <QStringList>

//
// Windows, and store applications in particular, change their binary paths on every single update,
// which makes the user decide the very same thing over and over again until they stop reading the
// prompts altogether. Rather than silently trusting any new path we look at the paths we have
// already made a decision about and check whether the new one differs in only one single spot.
// If so that spot, and only that spot, is turned into a wildcard, which yields a pattern that
// survives the next update without covering anything the user did not already trust.
//

struct SPathPattern
{
	bool IsValid() const					{ return !Pattern.isEmpty(); }

	QString Pattern;						// the derived pattern, empty when nothing could be derived
	QStringList Similar;					// the already known paths the pattern was derived from
	bool Guessed = false;					// read off the path alone, with nothing to compare it against
};

// Derives a pattern for Path from the given candidate paths. Returns an invalid result when no
// candidate is similar enough, or when the pattern would turn out too permissive to be of use.
SPathPattern MakePathPattern(const QString& Path, const QStringList& Candidates);

// With nothing to compare against: every dotted version number in a folder name gives way, its
// dots kept ("_1.21.2361.0_" becomes "_*.*.*.*_"). The file name and everything else stays, and a
// path with no such version comes back as it is, for the user to edit. Always valid.
SPathPattern GuessPathPattern(const QString& Path);

// Matches a path against a pattern, '?' and '*' do not cross a path separator, '**' does.
bool MatchPathPattern(const QString& Pattern, const QString& Path);
