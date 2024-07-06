#include "pch.h"
#include "XVariant.h"
#include "Exception.h"


void XVariantTest()
{

	XVariant test;
	test["test"] = 123;

	auto x = test["test"];
	XVariant num = x;
	auto snum = test["test"].AsQStr();
}

QString XVariant::ToQString() const
{
	auto pVal = Val(true);
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return QString();
	
	if (pVal.Value->Type == VAR_TYPE_UNICODE)
		return QString::fromWCharArray((wchar_t*)pVal.Value->Payload, pVal.Value->Size / sizeof(wchar_t));
	if (pVal.Value->Type == VAR_TYPE_ASCII || pVal.Value->Type == VAR_TYPE_BYTES)
		return QString::fromLatin1((char*)pVal.Value->Payload, pVal.Value->Size);
	if(pVal.Value->Type == VAR_TYPE_UTF8)
		return QString::fromUtf8((char*)pVal.Value->Payload, pVal.Value->Size);
	
	return QString();
}

bool XVariant::FromQVariant(const QVariant& qVariant, QFlags Flags)
{
	Clear();
	auto pVal = Val();

	switch(qVariant.type())
	{
		case QVariant::Map:
		{
			QVariantMap Map = qVariant.toMap();
			pVal.Value->Type = VAR_TYPE_MAP;
			pVal.Value->AllocContainer();
			for(QVariantMap::iterator I = Map.begin(); I != Map.end(); ++I)
			{
				std::string Name = I.key().toStdString();

				XVariant Temp;
				Temp.FromQVariant(I.value(), Flags);
				pVal.Value->Insert(Name.c_str(), &Temp);
			}
			break;
		}
		case QVariant::List:
		case QVariant::StringList:
		{
			QVariantList List = qVariant.toList();
			pVal.Value->Type = VAR_TYPE_LIST;
			pVal.Value->AllocContainer();
			for(QVariantList::iterator I = List.begin(); I != List.end(); ++I)
			{
				XVariant Temp;
				Temp.FromQVariant(*I, Flags);
				pVal.Value->Append(&Temp);
			}
			break;
		}
		case QVariant::Hash:
		{
			QVariantHash Map = qVariant.toHash();
			pVal.Value->Type = VAR_TYPE_MAP;
			pVal.Value->AllocContainer();
			for(QVariantHash::iterator I = Map.begin(); I != Map.end(); ++I)
			{
				//std::string Name = I.key().toStdString();
				//if (Name.length() > 4) {
				//	Q_ASSERT(0);
				//	Name.resize(4);
				//}
				//uint32 Index = 0;
				//memcpy(&Index, Name.c_str(), Name.size());
				uint32 Index = I.key().toUInt();

				XVariant Temp;
				Temp.FromQVariant(I.value(), Flags);
				pVal.Value->Insert(Index, &Temp);
			}
			break;
		}

		case QVariant::Bool:
		{
			bool Value = qVariant.toBool();
			pVal.Value->Init(VAR_TYPE_UINT, sizeof(Value), &Value);
			break;
		}
		//case QVariant::Char:
		case QVariant::Int:
		{
			sint32 Value = qVariant.toInt();
			pVal.Value->Init(VAR_TYPE_SINT, sizeof (sint32), &Value);
			break;
		}
		case QVariant::UInt:
		{
			uint32 Value = qVariant.toUInt();
			pVal.Value->Init(VAR_TYPE_UINT, sizeof (sint32), &Value);
			break;
		}
		case QVariant::LongLong:
		{
			sint64 Value = qVariant.toLongLong();
			pVal.Value->Init(VAR_TYPE_SINT, sizeof(Value), &Value);
			break;
		}
		case QVariant::ULongLong:
		{
			uint64 Value = qVariant.toULongLong();
			pVal.Value->Init(VAR_TYPE_UINT, sizeof(Value), &Value);
			break;
		}
		case QVariant::Double:
		{
			double Value = qVariant.toDouble();
			pVal.Value->Init(VAR_TYPE_DOUBLE, sizeof(Value), &Value);
			break;
		}
		case QVariant::String:
		{
			if (Flags & eUtf8) {
				QByteArray Value = qVariant.toString().toUtf8();
				pVal.Value->Init(VAR_TYPE_UTF8, Value.size(), Value.data());
			}
			else {
				std::wstring Value = qVariant.toString().toStdWString();
				pVal.Value->Init(VAR_TYPE_UNICODE, Value.size(), Value.data());
			}
			break;
		}
		case QVariant::ByteArray:
		{
			QByteArray Value = qVariant.toByteArray();
			pVal.Value->Init(VAR_TYPE_BYTES, Value.size(), Value.data());
			break;
		}
		//case QVariant::BitArray:
		//case QVariant::Date:
		//case QVariant::Time:
		//case QVariant::DateTime:
		//case QVariant::Url:
		default:
			ASSERT(0);
			return false;
	}
	return true;
}

QVariant XVariant::ToQVariant() const
{
	auto pVal = Val();

	switch(pVal.Value->Type)
	{
		case VAR_TYPE_MAP:
		{
			QVariantMap Map;
			auto Ret = m_Variant->Map();
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
			{
#ifndef VAR_NO_STL
				QString Name = QString::fromLatin1(I->first);
				Map[Name] = (*(XVariant*)&I->second).ToQVariant();
#else
				QString Name = QString::fromLatin1(I.Key().ConstData());
				Map[Name] = (*(XVariant*)&I.Value()).ToQVariant();
#endif
			}
			return Map;
		}
		case VAR_TYPE_LIST:
		{
			QVariantList List;
			auto Ret = m_Variant->List();
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
				List.append((*(XVariant*)&(*I)).ToQVariant());
			return List;
		}
		case VAR_TYPE_INDEX:
		{
			QVariantHash Map;
			auto Ret = m_Variant->IMap();
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
			{
#ifndef VAR_NO_STL
				//QString Name = QString::fromLatin1(I->first.c, 4);
				QString Name = "X" + QString::number(I->first.u, 16).rightJustified(8, '0');
				Map[Name] = (*(XVariant*)&I->second).ToQVariant();
#else
				//QString Name = QString::fromLatin1(I.Key().c, 4);
				QString Name = "X" + QString::number(I.Key().u, 16).rightJustified(8, '0');
				Map[Name] = (*(XVariant*)&I.Key()).ToQVariant();
#endif
			}
			return Map;
		}

		case VAR_TYPE_BYTES:
			return QByteArray((char*)pVal.Value->Payload, pVal.Value->Size);

		case VAR_TYPE_UNICODE:
			return QString::fromWCharArray((wchar_t*)pVal.Value->Payload, pVal.Value->Size / sizeof(wchar_t));
		case VAR_TYPE_UTF8:
			return QString::fromUtf8((char*)pVal.Value->Payload, pVal.Value->Size);
		case VAR_TYPE_ASCII:
			return QString::fromLatin1((char*)pVal.Value->Payload, pVal.Value->Size);

		case VAR_TYPE_SINT:
			if(pVal.Value->Size <= sizeof(sint8))
				return sint8(*this);
			else if(pVal.Value->Size <= sizeof(sint16))
				return sint16(*this);
			else if(pVal.Value->Size <= sizeof(sint32))
				return sint32(*this);
			else if(pVal.Value->Size <= sizeof(uint64))
				return sint64(*this);
			else
				return QByteArray((char*)pVal.Value->Payload, pVal.Value->Size);
		case VAR_TYPE_UINT:
			if(pVal.Value->Size <= sizeof(uint8))
				return uint8(*this);
			else if(pVal.Value->Size <= sizeof(uint16))
				return uint16(*this);
			else if(pVal.Value->Size <= sizeof(uint32))
				return uint32(*this);
			else if(pVal.Value->Size <= sizeof(uint64))
				return uint64(*this);
			else
				return QByteArray((char*)pVal.Value->Payload, pVal.Value->Size);

		case VAR_TYPE_DOUBLE:
			return double(*this);

		case VAR_TYPE_EMPTY:
		default:
			return QVariant();
	}
}

QString XVariant::SerializeXml() const
{
	QString String;
	QXmlStreamWriter xml(&String);
	Serialize(*this, xml);
	return String;
}

bool XVariant::ParseXml(const QString& String)
{
	QXmlStreamReader xml(String);
	bool bOk = false;
	*this = Parse(xml, &bOk);
	return bOk;
}


void XVariant::Serialize(const XVariant& Variant, QXmlStreamWriter &xml)
{
#ifdef _DEBUG
	xml.setAutoFormatting(true);
#endif
	xml.writeStartDocument();
	Serialize("Variant", Variant, xml);
	xml.writeEndDocument();
}

XVariant XVariant::Parse(QXmlStreamReader &xml, bool* pOK)
{
	XVariant Variant;
	QString Temp;
	bool bOk = Parse(Temp, Variant, xml);
	if(pOK) * pOK = bOk;
	return Variant;
}

void XVariant::Serialize(const QString& Name, const XVariant& Variant, QXmlStreamWriter &xml)
{
	auto pVal = Variant.Val();

	xml.writeStartElement(Name);
	xml.writeAttribute("Type", GetTypeStr(pVal.Value->Type));

	switch(pVal.Value->Type)
	{
	case VAR_TYPE_EMPTY:
		break;
	case VAR_TYPE_MAP:
	{
		auto Ret = Variant.m_Variant->Map();
#ifndef VAR_NO_STL
		xml.writeAttribute("Count", QString::number(Ret.Value->size()));
#else
		xml.writeAttribute("Count", QString::number(Ret.Value->Count()));
#endif
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
		{
#ifndef VAR_NO_STL
			QString Name = QString::fromLatin1(I->first);
			Serialize(Name, *(XVariant*)&I->second, xml);
#else
			QString Name = QString::fromLatin1(I.Key().ConstData());
			Serialize(Name, *(XVariant*)&I.Value(), xml);
#endif
		}
		break;
	}
	case VAR_TYPE_LIST:
	{
		auto Ret = Variant.m_Variant->List();
#ifndef VAR_NO_STL
		xml.writeAttribute("Count", QString::number(Ret.Value->size()));
#else
		xml.writeAttribute("Count", QString::number(Ret.Value->Count()));
#endif
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
			Serialize("Variant", *(XVariant*)&(*I), xml);
		break;
	}
	case VAR_TYPE_INDEX:
	{
		auto Ret = Variant.m_Variant->IMap();
#ifndef VAR_NO_STL
		xml.writeAttribute("Count", QString::number(Ret.Value->size()));
#else
		xml.writeAttribute("Count", QString::number(Ret.Value->Count()));
#endif
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
		{
#ifndef VAR_NO_STL
			//QString Name = QString::fromLatin1(I->first.c, 4);
			QString Name = "X" + QString::number(I->first.u, 16).rightJustified(8, '0');
			Serialize(Name, *(XVariant*)&I->second, xml);
#else
			//QString Name = QString::fromLatin1(I.Key().c, 4);
			QString Name = "X" + QString::number(I.Key().u, 16).rightJustified(8, '0');
			Serialize(Name, *(XVariant*)&I.Value(), xml);
#endif
		}
		break;
	}

	case VAR_TYPE_BYTES:
		xml.writeAttribute("Size", QString::number(pVal.Value->Size));
		xml.writeCharacters (QByteArray((char*)pVal.Value->Payload, pVal.Value->Size).toBase64());
		break;

	case VAR_TYPE_UNICODE:
	case VAR_TYPE_UTF8: {
		QString Text = Variant.ToQString();
		xml.writeAttribute("Length", QString::number(Text.length()));
		xml.writeCharacters(Text.toUtf8().toPercentEncoding(" :;/|,'+()"));
		break;
	}
	case VAR_TYPE_ASCII: {
		xml.writeAttribute("Length", QString::number(Variant.GetSize()));
		xml.writeCharacters(Variant.AsQBytes().toPercentEncoding(" :;/|,'+()"));
		break;
	}

	case VAR_TYPE_SINT:
	case VAR_TYPE_UINT:
	case VAR_TYPE_DOUBLE:
		xml.writeAttribute("Size", QString::number(Variant.GetSize()));
		xml.writeCharacters(Variant.AsQStr());
		break;

	default:
		ASSERT(0);
	}
	xml.writeEndElement();
}

bool XVariant::Parse(QString &Name, XVariant &Variant, QXmlStreamReader &xml)
{
	Variant.Clear();

	bool bOpen = false;
	int Type = VAR_TYPE_INVALID;
	quint64 Size = 0;
	QString Text;
	while (!xml.atEnd())
	{
		xml.readNext();
		if (xml.error()) {
			qDebug() << xml.errorString();
			break;
		}
		if (xml.isEndDocument())
			continue;

		if (xml.isStartElement())
		{
			bOpen = true;

			Name = xml.name().toString();
			
			Type = GetTypeValue(xml.attributes().value("Type").toString());
			
			auto aSize = xml.attributes().value("Size");
			Size = aSize.isNull() ? 0 : aSize.toString().toULongLong();

			QString Key;
			XVariant Item;
			switch(Type)
			{
			case VAR_TYPE_MAP:
			{
				XVariant Map;
				while(Parse(Key, Item, xml))
					Map[Key.toStdString().c_str()] = Item;
				Variant = Map;
				return true;
			}
			case VAR_TYPE_INDEX:
			{
				XVariant Hash;
				while (Parse(Key, Item, xml)) {
					uint32 Index = Key.mid(1).toUInt(NULL, 16);
					Hash[Index] = Item;
				}
				Variant = Hash;
				return true;
			}
			case VAR_TYPE_LIST:
			{
				XVariant List;
				while(Parse(Key, Item, xml))
					List.Append(Item);
				Variant = List;
				return true;
			}
			}
		}
		else if (xml.isCharacters())
		{
			if(bOpen)
				Text.append(xml.text().toString());
		}
		else if (xml.isEndElement())
		{
			if(!bOpen)
				return false;
			
			switch (Type)
			{
			case VAR_TYPE_EMPTY:
				Variant = CVariant(NULL, 0, VAR_TYPE_EMPTY);
				break;

			case VAR_TYPE_BYTES:
				Variant = QByteArray::fromBase64(Text.toLatin1());
				break;

			case VAR_TYPE_UNICODE:
				Variant = CVariant(QString::fromUtf8(QByteArray::fromPercentEncoding(Text.toLatin1())).toStdWString(), Type == VAR_TYPE_UTF8);
				break;
			case VAR_TYPE_ASCII:
				Variant = CVariant(QString::fromLatin1(QByteArray::fromPercentEncoding(Text.toLatin1())).toStdString());
				break;

			case VAR_TYPE_SINT: {
				qlonglong i64 = Text.toLongLong();
				if(Size > sizeof(i64)) Size = sizeof(i64);
				Variant.InitValue(VAR_TYPE_SINT, Size, &i64);
				break;
			}

			case VAR_TYPE_UINT: {
				qulonglong i64 = Text.toULongLong();
				if(Size > sizeof(i64)) Size = sizeof(i64);
				Variant.InitValue(VAR_TYPE_UINT, Size, &i64);
				break;
			}

			case VAR_TYPE_DOUBLE:
				Variant = CVariant(Text.toDouble());
				break;

			default:
				ASSERT(0);
			}
				
			return true;
		}
	}

	qDebug() << "incomplete XML";
	return false;
}

QString XVariant::GetTypeStr(int Type)
{
	switch (Type)
	{
	case VAR_TYPE_EMPTY:	return "Empty";
	case VAR_TYPE_MAP:		return "Map";
	case VAR_TYPE_LIST:		return "List";
	case VAR_TYPE_INDEX:	return "Index";
	case VAR_TYPE_BYTES:	return "Bytes";
	case VAR_TYPE_ASCII:	return "Ascii";
	case VAR_TYPE_UTF8:		return "Utf8";
	case VAR_TYPE_UNICODE:	return "Unicode";
	case VAR_TYPE_UINT:		return "UInt";
	case VAR_TYPE_SINT:		return "SInt";
	case VAR_TYPE_DOUBLE:	return "Double";
	case VAR_TYPE_CUSTOM:	return "Custom";
	case VAR_TYPE_INVALID:
	default:				return "Invalid";
	}
}

int XVariant::GetTypeValue(QString Type)
{
	if(Type == "Empty")		return VAR_TYPE_EMPTY;
	if(Type == "Map")		return VAR_TYPE_MAP;
	if(Type == "List")		return VAR_TYPE_LIST;
	if(Type == "Index")		return VAR_TYPE_INDEX;
	if(Type == "Bytes")		return VAR_TYPE_BYTES;
	if(Type == "Ascii")		return VAR_TYPE_ASCII;
	if(Type == "Utf8")		return VAR_TYPE_UTF8;
	if(Type == "Unicode")	return VAR_TYPE_UNICODE;
	if(Type == "UInt")		return VAR_TYPE_UINT;
	if(Type == "SInt")		return VAR_TYPE_SINT;
	if(Type == "Double")	return VAR_TYPE_DOUBLE;
	if(Type == "Custom")	return VAR_TYPE_CUSTOM;
	return VAR_TYPE_INVALID;
}