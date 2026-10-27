#include "pch.h"
#include "PathPattern.h"
#include <QRegularExpression>

//
// Only the one path segment which actually differs is touched, and within that segment only whole
// fields are replaced, never a part of one. A field is what lies between two separators, and which
// separator delimits the fields is decided per segment by trying all of them and keeping the result
// which preserves the longest literal head, as that head is what identifies the publisher and the
// application. "Microsoft.WindowsStore_22401.1401.4.0_x64__8wekyb3d8bbwe" hence yields
// "Microsoft.WindowsStore_*_x64__8wekyb3d8bbwe" and not "*.0.0_x64__8wekyb3d8bbwe", even though the
// latter would keep more literal characters overall.
//
// The wildcard is kept as narrow as the observed difference allows, so the shape of what moved
// survives: "app-1.0.9015" versus "app-1.0.9016" becomes "app-1.*.*" and a store version field
// becomes "*.*.*" rather than a bare "*", which would take any text at all.
//
// Within a version every number gives way, never only the one seen to move. A template is written
// so that the next release needs no question, and "1.0.*" stops covering the moment 1.0.9 becomes
// 1.1.0, which is the very update it was written for.
//

static const char* PathPattern__Separators = "_-. +~,@";

// a bare wildcard is only acceptable where the segment is a version of some sort, "1.2.3", "v17", "22401"
static bool PathPattern__IsVersionLike(const QString& Segment)
{
	static const QRegularExpression Version("^[vV]?[0-9][0-9A-Za-z.+_-]*$");
	return Version.match(Segment).hasMatch();
}

//
// A version number and nothing else: digits and dots, optionally announced by a v. Deliberately
// stricter than IsVersionLike above, which only asks that a segment start with a digit and so
// happily accepts a store publisher hash like "8wekyb3d8bbwe". Here the answer decides whether a
// field is the sort of thing that moves between releases, and a publisher is not.
//
static bool PathPattern__IsVersionNumber(const QString& Field)
{
	static const QRegularExpression Number("^[vV]?[0-9]+([._][0-9]+)*$");
	return Number.match(Field).hasMatch();
}

//
// What to put in place of a version that moved. A bare "*" would take anything at all, so where
// both sides are dotted numbers of the same shape the dots are kept and only the numbers give way:
// "1.0.1" against "1.0.0" becomes "*.*.*", which still insists on something version shaped.
//
// Every number goes, never just the one that moved. A rule template is there so that the next
// release is covered without asking, and "1.0.*" stops covering the moment 1.0.9 turns into 1.1.0,
// which is exactly the update it was written for.
//
static QString PathPattern__VersionShape(const QString& Left, const QString& Right)
{
	static const QRegularExpression Dotted("^[0-9]+(\\.[0-9]+)+$");
	if (!Dotted.match(Left).hasMatch() || !Dotted.match(Right).hasMatch())
		return QString();

	int Parts = (int)Left.count('.') + 1;
	if (Parts != (int)Right.count('.') + 1)
		return QString(); // not the same shape, so there is no shape to keep

	QStringList Shape;
	for (int i = 0; i < Parts; i++)
		Shape.append("*");
	return Shape.join('.');
}

static bool PathPattern__IsNumber(const QString& Field)
{
	if (Field.isEmpty())
		return false;
	for (int i = 0; i < Field.length(); i++) {
		if (Field[i] < '0' || Field[i] > '9')
			return false;
	}
	return true;
}

//
// Builds the pattern for the one segment which differs between the two paths.
//
static QString PathPattern__MakeSegment(const QString& Left, const QString& Right, bool bFileName, bool* pbVersionOnly = nullptr, int* pGivenUp = nullptr)
{
	QString Best;
	int BestHead = -1;		// literal characters in front of the first wildcard
	int BestLiterals = -1;	// literal characters in total
	bool BestAnchored = false;
	bool BestVersions = false;	// everything the wildcards swallowed was a version number
	int BestGivenUp = 0;		// how many fields were given up for it
	bool BestCut = false;		// this level cut a version in two, see below

	if (pbVersionOnly) *pbVersionOnly = false;
	if (pGivenUp) *pGivenUp = 1;

	const bool bVersion = PathPattern__IsVersionLike(Left) && PathPattern__IsVersionLike(Right);

	for (const char* pSeparator = PathPattern__Separators; *pSeparator; pSeparator++)
	{
		QChar Separator = QChar::fromLatin1(*pSeparator);

		QStringList L = Left.split(Separator);
		QStringList R = Right.split(Separator);
		if (L.count() < 2 || L.count() != R.count())
			continue; // this separator does not structure both segments the same way

		QVector<bool> Differs(L.count());
		for (int i = 0; i < L.count(); i++)
			Differs[i] = (L[i].compare(R[i], Qt::CaseInsensitive) != 0);

		//
		// A dotted number is one single version, not a handful of fields which happen to sit next
		// to each other, so a component which is equal on both sides says nothing once an earlier
		// one has already moved. Every run of plain numbers is therefore all or nothing. Without
		// this, "1.0.2.3" against "5.0.7.9" comes out as "*.0.*.*", which stops matching the very
		// moment that nought turns into a one.
		//
		//
		// A run which moved but is flanked by digits stuck to a neighbouring field was cut out of a
		// longer version by a separator which also occurs in the name. Splitting
		// "Microsoft.App_1.2.3.0_x64__8wekyb3d8bbwe" on '.' leaves "App_1" and "0_x64..." standing as
		// literal fields, so their 1 and 0 count as kept text and that level would win, yielding
		// "1.*.*.0", which breaks at the next major version. Such a level is only taken when no other
		// one keeps the version whole.
		//
		bool bCut = false;
		auto EndsWithDigit = [](const QString& s) { return !s.isEmpty() && s.back().isDigit(); };
		auto StartsWithDigit = [](const QString& s) { return !s.isEmpty() && s.front().isDigit(); };

		for (int i = 0; i < L.count(); )
		{
			if (!PathPattern__IsNumber(L[i]) || !PathPattern__IsNumber(R[i])) {
				i++;
				continue;
			}
			int End = i;
			while (End + 1 < L.count() && PathPattern__IsNumber(L[End + 1]) && PathPattern__IsNumber(R[End + 1]))
				End++;

			bool bRunMoved = false;
			for (int j = i; j <= End; j++) {
				if (Differs[j])
					bRunMoved = true;
			}
			if (bRunMoved) {
				for (int j = i; j <= End; j++)
					Differs[j] = true;
				if ((i > 0 && EndsWithDigit(L[i - 1])) || (End + 1 < L.count() && StartsWithDigit(L[End + 1])))
					bCut = true;
			}
			i = End + 1;
		}

		QStringList Fields;
		int Literals = L.count() - 1; // the separators themselves are literal as well
		bool bAnchored = false;
		bool bVersions = true;
		int GivenUp = 0;
		for (int i = 0; i < L.count(); i++)
		{
			if (Differs[i]) {
				// a version keeps its dots, anything else has nothing worth keeping
				QString Shape = PathPattern__VersionShape(L[i], R[i]);
				Fields.append(Shape.isEmpty() ? QString("*") : Shape);
				Literals += (int)Shape.count('.');
				GivenUp++;
				if (!PathPattern__IsVersionNumber(L[i]) || !PathPattern__IsVersionNumber(R[i]))
					bVersions = false;
				continue;
			}
			Fields.append(L[i]);
			Literals += L[i].length();
			if (!L[i].isEmpty())
				bAnchored = true;
		}

		//
		// Normally a level which keeps nothing literal is of no use. A version is the exception:
		// there the shape is all there is to keep, and "*.*.*" still insists on three components
		// where a bare "*" would take anything at all.
		//
		if (!bAnchored && !bVersion)
			continue;

		QString Pattern = Fields.join(Separator);
		int Head = (int)Pattern.indexOf('*');

		//
		// The most literal characters kept wins, because that is the same as the wildcards having
		// the least to swallow. Judging by the literal head alone picks the wrong level whenever a
		// field which differs as a whole sits at the end: splitting "FwTestApp_1.0.1_x64__testsig"
		// on '.' keeps a longer head than splitting it on '_' does, but it throws away the whole
		// "_x64__testsig" tail with it, and a pattern without that tail covers another architecture.
		// The head only settles a tie, where it tells the better anchored of two equals apart.
		//
		if (!Best.isEmpty() && bCut != BestCut) {
			if (bCut)
				continue; // a level which keeps the version whole beats one which cuts it
		}
		else if (Literals < BestLiterals || (Literals == BestLiterals && Head <= BestHead))
			continue;
		Best = Pattern;
		BestHead = Head;
		BestLiterals = Literals;
		BestAnchored = bAnchored;
		BestVersions = bVersions;
		BestGivenUp = GivenUp;
		BestCut = bCut;
	}

	if (Best.isEmpty())
	{
		// the segment has no inner structure to go by, so it is all or nothing
		if (bFileName || !bVersion)
			return QString();
		if (pbVersionOnly) *pbVersionOnly = true; // the whole segment was the version
		return "*";
	}

	if (!BestAnchored)
	{
		// a version and nothing else may come out of this without a single literal field
		if (bFileName || !bVersion)
			return QString();
	}
	else if (BestLiterals < 3) // too little left to be an anchor of any value
		return QString();

	if (bFileName)
	{
		// a name must keep a literal head, or the pattern would cover every binary of the directory
		if (BestHead < 3)
			return QString();

		// and the extension pins down what kind of file we are talking about
		int iDot = (int)Left.lastIndexOf('.');
		if (iDot == -1 || !Best.endsWith(Left.mid(iDot), Qt::CaseInsensitive))
			return QString();
	}

	if (pbVersionOnly) *pbVersionOnly = BestVersions;
	if (pGivenUp) *pGivenUp = BestGivenUp;
	return Best;
}

//
// Compares the path against one single candidate, the two must differ in exactly one segment,
// anything further apart than that is not a version change and must not be guessed at.
//
static QString PathPattern__MakePair(const QStringList& Parts, const QString& Other, bool* pbVersionOnly = nullptr, int* pGivenUp = nullptr)
{
	QStringList OtherParts = Other.split('\\');
	if (OtherParts.count() != Parts.count())
		return QString();

	int iDiff = -1;
	for (int i = 0; i < Parts.count(); i++) {
		if (Parts[i].compare(OtherParts[i], Qt::CaseInsensitive) == 0)
			continue;
		if (iDiff != -1)
			return QString(); // more than one segment differs
		iDiff = i;
	}

	if (iDiff <= 0) // the paths are equal, or only the drive differs
		return QString();

	QString Segment = PathPattern__MakeSegment(Parts[iDiff], OtherParts[iDiff], iDiff == Parts.count() - 1, pbVersionOnly, pGivenUp);
	if (Segment.isEmpty())
		return QString();

	QStringList Pattern = Parts;
	Pattern[iDiff] = Segment;
	return Pattern.join('\\');
}

SPathPattern MakePathPattern(const QString& Path, const QStringList& Candidates)
{
	SPathPattern Result;

	if (Path.isEmpty() || Path.contains('*') || Path.contains('?'))
		return Result;

	QStringList Parts = Path.split('\\');
	if (Parts.count() < 3) // a pattern needs a directory to be anchored in
		return Result;

	// group by derived pattern, so that every path backing the same conclusion counts once
	QMap<QString, QStringList> Patterns;
	QMap<QString, bool> VersionOnly;	// did this pattern come of a version moving
	QMap<QString, int> GivenUp;			// and how many fields did it cost
	foreach(const QString& Candidate, Candidates)
	{
		if (Candidate.isEmpty() || Candidate.contains('*') || Candidate.contains('?'))
			continue; // only concrete paths tell us anything about what actually varies

		bool bVersionOnly = false;
		int iGivenUp = 1;
		QString Pattern = PathPattern__MakePair(Parts, Candidate, &bVersionOnly, &iGivenUp);
		if (Pattern.isEmpty())
			continue;

		if (!VersionOnly.contains(Pattern)) {
			VersionOnly.insert(Pattern, bVersionOnly);
			GivenUp.insert(Pattern, iGivenUp);
		}

		QStringList& Similar = Patterns[Pattern];
		if (!Similar.contains(Candidate, Qt::CaseInsensitive))
			Similar.append(Candidate);
	}

	//
	// How many fields the pattern gives up decides first. Every wildcard is one more place where
	// the pattern stops telling programs apart, so the one that gives up the fewest says the most
	// about what it covers. Going by how many known paths back a pattern instead lets a crowd
	// decide: unrelated programs which merely share a binary name and a directory depth each sit
	// one segment away from this path just as a real sibling does, and together they outvote it
	// with a pattern that has dropped the application name altogether.
	//
	// Among patterns which give up just as much, the one whose wildcard stands where a version
	// number used to be wins. A template exists because versions move; a name, an architecture or
	// a publisher does not, and a pattern which swallowed one of those covers programs which were
	// never the subject at all. Counting how many known paths agree says the same thing only by
	// coincidence: it is right while the versions outnumber the architectures and wrong the moment
	// a second architecture is installed, which is too close to chance for a security decision.
	//
	// Where that does not separate them either, more agreeing paths is still better than fewer,
	// and more literal text better than less.
	//
	int BestStars = -1, BestLiterals = -1;
	bool BestVersion = false;
	for (auto I = Patterns.begin(); I != Patterns.end(); ++I)
	{
		// how many fields were given up, not how many asterisks that took: a version keeps its
		// dots and so spends several of them on what is still one single thing let go of
		int Stars = GivenUp.value(I.key(), 1);
		int Literals = (int)I.key().length() - (int)I.key().count('*');
		bool bVersion = VersionOnly.value(I.key(), false);
		if (Result.IsValid())
		{
			if (Stars > BestStars)
				continue;
			if (Stars == BestStars)
			{
				if (BestVersion && !bVersion)
					continue;
				if (BestVersion == bVersion)
				{
					if (I.value().count() < Result.Similar.count())
						continue;
					if (I.value().count() == Result.Similar.count() && Literals <= BestLiterals)
						continue;
				}
			}
		}
		Result.Pattern = I.key();
		Result.Similar = I.value();
		BestStars = Stars;
		BestLiterals = Literals;
		BestVersion = bVersion;
	}

	return Result;
}

//
// The first time a store app asks there is no earlier version to hold it against, yet the user
// may already know they want every version of it decided the same way. The one thing that tells
// a version apart without a comparison is its shape: numbers joined by dots. Only that is taken,
// and only in the folders, which is where a store or squirrel style update moves a program to;
// it is wildcarded the way a deduced version would be, every number gone and the dots kept.
//
// A lone number is left alone, "Office16" or "x64" are names, not releases, and so is anything
// glued to a digit or a dot on either side, so "8wekyb3d8bbwe" never yields a wildcard. What
// comes out is a suggestion, shown in an editor before anything is saved.
//
SPathPattern GuessPathPattern(const QString& Path)
{
	static const QRegularExpression Version("(?<![0-9.])[0-9]+(?:\\.[0-9]+)+(?![0-9.])");
	static const QRegularExpression Number("[0-9]+");

	QStringList Parts = Path.split('\\');
	for (int i = 0; i < Parts.size() - 1; i++) // the last one is the file name, it stays
	{
		QString& Part = Parts[i];
		QList<QRegularExpressionMatch> Matches;
		for (auto I = Version.globalMatch(Part); I.hasNext(); )
			Matches.append(I.next());
		for (int j = (int)Matches.size() - 1; j >= 0; j--) { // back to front, so the offsets stay true
			QString Shape = Matches[j].captured();
			Shape.replace(Number, "*");
			Part.replace(Matches[j].capturedStart(), Matches[j].capturedLength(), Shape);
		}
	}

	SPathPattern Result;
	Result.Pattern = Parts.join('\\');
	Result.Guessed = true;
	return Result;
}

bool MatchPathPattern(const QString& Pattern, const QString& Path)
{
	if (Pattern.isEmpty())
		return false;

	auto Equal = [](QChar Left, QChar Right) { return Left.toLower() == Right.toLower(); };

	int iPat = 0, iStr = 0;
	int iStarPat = -1, iStarStr = -1;
	bool bStarCrosses = false;

	while (iStr < Path.length())
	{
		if (iPat < Pattern.length() && Pattern[iPat] == '*')
		{
			bStarCrosses = (iPat + 1 < Pattern.length() && Pattern[iPat + 1] == '*');
			iStarPat = iPat;
			iStarStr = iStr;
			iPat += bStarCrosses ? 2 : 1;
			continue;
		}

		if (iPat < Pattern.length() && ((Pattern[iPat] == '?' && Path[iStr] != '\\') || Equal(Pattern[iPat], Path[iStr])))
		{
			iPat++;
			iStr++;
			continue;
		}

		if (iStarPat == -1)
			return false;

		if (!bStarCrosses && Path[iStarStr] == '\\') // a single wildcard does not cross a separator
			return false;

		iStarStr++;
		iStr = iStarStr;
		iPat = iStarPat + (bStarCrosses ? 2 : 1);
	}

	while (iPat < Pattern.length() && Pattern[iPat] == '*')
		iPat++;
	return iPat == Pattern.length();
}
