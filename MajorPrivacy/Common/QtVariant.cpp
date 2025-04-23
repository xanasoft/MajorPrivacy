#include "pch.h"
#include "QtVariant.h"

QString QtVariant::ToQString() const
{
	if(m_Type == VAR_TYPE_EMPTY)
		return QString();

	if (m_Type == VAR_TYPE_UNICODE)
		return QString::fromWCharArray((wchar_t*)GetData(), GetSize() / sizeof(wchar_t));
	if (m_Type == VAR_TYPE_ASCII || m_Type == VAR_TYPE_BYTES)
		return QString::fromLatin1((char*)GetData(), GetSize());
	if(m_Type == VAR_TYPE_UTF8)
		return QString::fromUtf8((char*)GetData(), GetSize());
	return QString();
}

bool QtVariant::FromQVariant(const QVariant& qVariant, QFlags Flags)
{
	/*Clear();
	auto pVal = Val();

	switch(qVariant.type())
	{
	case QVariant::Map:
	{
		QVariantMap Map = qVariant.toMap();
		m_Type = VAR_TYPE_MAP;
		pVal.Value->Container.Map = AllocMap(m_pMem);
		for(QVariantMap::iterator I = Map.begin(); I != Map.end(); ++I)
		{
			std::string Name = I.key().toStdString();

			QtVariant Temp;
			Temp.FromQVariant(I.value(), Flags);
			Insert(m_pMem, pVal.Value, Name.c_str(), &Temp);
		}
		break;
	}
	case QVariant::List:
	case QVariant::StringList:
	{
		QVariantList List = qVariant.toList();
		m_Type = VAR_TYPE_LIST;
		pVal.Value->Container.List = AllocList(m_pMem);
		for(QVariantList::iterator I = List.begin(); I != List.end(); ++I)
		{
			QtVariant Temp;
			Temp.FromQVariant(*I, Flags);
			Append(m_pMem, pVal.Value, &Temp);
		}
		break;
	}
	case QVariant::Hash:
	{
		QVariantHash Map = qVariant.toHash();
		m_Type = VAR_TYPE_MAP;
		pVal.Value->Container.Map = AllocMap(m_pMem);
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

			QtVariant Temp;
			Temp.FromQVariant(I.value(), Flags);
			Insert(m_pMem, pVal.Value, Index, &Temp);
		}
		break;
	}

	case QVariant::Bool:
	{
		bool Value = qVariant.toBool();
		Init(m_pMem, pVal.Value, VAR_TYPE_UINT, sizeof(Value), &Value);
		break;
	}
	//case QVariant::Char:
	case QVariant::Int:
	{
		sint32 Value = qVariant.toInt();
		Init(m_pMem, pVal.Value, VAR_TYPE_SINT, sizeof (sint32), &Value);
		break;
	}
	case QVariant::UInt:
	{
		uint32 Value = qVariant.toUInt();
		Init(m_pMem, pVal.Value, VAR_TYPE_UINT, sizeof (sint32), &Value);
		break;
	}
	case QVariant::LongLong:
	{
		sint64 Value = qVariant.toLongLong();
		Init(m_pMem, pVal.Value, VAR_TYPE_SINT, sizeof(Value), &Value);
		break;
	}
	case QVariant::ULongLong:
	{
		uint64 Value = qVariant.toULongLong();
		Init(m_pMem, pVal.Value, VAR_TYPE_UINT, sizeof(Value), &Value);
		break;
	}
	case QVariant::Double:
	{
		double Value = qVariant.toDouble();
		Init(m_pMem, pVal.Value, VAR_TYPE_DOUBLE, sizeof(Value), &Value);
		break;
	}
	case QVariant::String:
	{
		if (Flags & eUtf8) {
			QByteArray Value = qVariant.toString().toUtf8();
			Init(m_pMem, pVal.Value, VAR_TYPE_UTF8, Value.size(), Value.data());
		}
		else {
			std::wstring Value = qVariant.toString().toStdWString();
			Init(m_pMem, pVal.Value, VAR_TYPE_UNICODE, Value.size(), Value.data());
		}
		break;
	}
	case QVariant::ByteArray:
	{
		QByteArray Value = qVariant.toByteArray();
		Init(m_pMem, pVal.Value, VAR_TYPE_BYTES, Value.size(), Value.data());
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
	}*/
	return true;
}

QVariant QtVariant::ToQVariant() const
{
	switch(m_Type)
	{
	case VAR_TYPE_MAP:
	{
		QVariantMap Map;
		auto Ret = GetMap(true, false);
		for (auto I = MapBegin(Ret.Value); I != MapEnd(); ++I)
		{
			QString Name = QString::fromLatin1(I.Key().ConstData());
			Map[Name] = (*(QtVariant*)&I.Value()).ToQVariant();
		}
		return Map;
	}
	case VAR_TYPE_LIST:
	{
		QVariantList List;
		auto Ret = GetList(true, false);
		for (uint32 i = 0; i < Ret.Value->Count; i++)
			List.append(((QtVariant*)Ret.Value->Data)[i].ToQVariant());
		return List;
	}
	case VAR_TYPE_INDEX:
	{
		QVariantHash Map;
		auto Ret = GetIndex(true, false);
		for (auto I = IndexBegin(Ret.Value); I != IndexEnd(); ++I)
		{
			//QString Name = QString::fromLatin1(I.Key().c, 4);
			QString Name = "X" + QString::number(I.Key().u, 16).rightJustified(8, '0');
			Map[Name] = (*(QtVariant*)&I.Key()).ToQVariant();
		}
		return Map;
	}

	case VAR_TYPE_BYTES:
		return QByteArray((char*)GetData(), GetSize());

	case VAR_TYPE_UNICODE:
		return QString::fromWCharArray((wchar_t*)GetData(), GetSize() / sizeof(wchar_t));
	case VAR_TYPE_UTF8:
		return QString::fromUtf8((char*)GetData(), GetSize());
	case VAR_TYPE_ASCII:
		return QString::fromLatin1((char*)GetData(), GetSize());

	case VAR_TYPE_SINT:
		if(GetSize() <= sizeof(sint8))
			return sint8(*this);
		else if(GetSize() <= sizeof(sint16))
			return sint16(*this);
		else if(GetSize() <= sizeof(sint32))
			return sint32(*this);
		else if(GetSize() <= sizeof(uint64))
			return sint64(*this);
		else
			return QByteArray((char*)GetData(), GetSize());
	case VAR_TYPE_UINT:
		if(GetSize() <= sizeof(uint8))
			return uint8(*this);
		else if(GetSize() <= sizeof(uint16))
			return uint16(*this);
		else if(GetSize() <= sizeof(uint32))
			return uint32(*this);
		else if(GetSize() <= sizeof(uint64))
			return uint64(*this);
		else
			return QByteArray((char*)GetData(), GetSize());

	case VAR_TYPE_DOUBLE:
		return double(*this);

	case VAR_TYPE_EMPTY:
	default:
		return QVariant();
	}
}

QString QtVariant::SerializeXml() const
{
	QString String;
	QXmlStreamWriter xml(&String);
	Serialize(*this, xml);
	return String;
}

bool QtVariant::ParseXml(const QString& String)
{
	QXmlStreamReader xml(String);
	bool bOk = false;
	*this = Parse(xml, &bOk);
	return bOk;
}


void QtVariant::Serialize(const QtVariant& Variant, QXmlStreamWriter &xml)
{
#ifdef _DEBUG
	xml.setAutoFormatting(true);
#endif
	xml.writeStartDocument();
	Serialize("Variant", Variant, xml);
	xml.writeEndDocument();
}

QtVariant QtVariant::Parse(QXmlStreamReader &xml, bool* pOK)
{
	QtVariant Variant;
	QString Temp;
	bool bOk = Parse(Temp, Variant, xml);
	if(pOK) * pOK = bOk;
	return Variant;
}

void QtVariant::Serialize(const QString& Name, const QtVariant& Variant, QXmlStreamWriter &xml)
{
	xml.writeStartElement(Name);
	xml.writeAttribute("Type", GetTypeStr(Variant.m_Type));

	switch(Variant.m_Type)
	{
	case VAR_TYPE_EMPTY:
		break;
	case VAR_TYPE_MAP:
		{
		auto Ret = Variant.GetMap(true, false);
		xml.writeAttribute("Count", QString::number(Ret.Value->Count));
		for (auto I = MapBegin(Ret.Value); I != MapEnd(); ++I)
		{
			QString Name = QString::fromLatin1(I.Key().ConstData());
			Serialize(Name, *(QtVariant*)&I.Value(), xml);
		}
		break;
	}
	case VAR_TYPE_LIST:
		{
		auto Ret = Variant.GetList(true, false);
		xml.writeAttribute("Count", QString::number(Ret.Value->Count));
		for (uint32 i = 0; i < Ret.Value->Count; i++)
			Serialize("Variant", ((QtVariant*)Ret.Value->Data)[i], xml);
		break;
	}
	case VAR_TYPE_INDEX:
		{
		auto Ret = Variant.GetIndex(true, false);
		xml.writeAttribute("Count", QString::number(Ret.Value->Count));
		for (auto I = IndexBegin(Ret.Value); I != IndexEnd(); ++I)
		{
			//QString Name = QString::fromLatin1(I.Key().c, 4);
			QString Name = "X" + QString::number(I.Key().u, 16).rightJustified(8, '0');
			Serialize(Name, *(QtVariant*)&I.Value(), xml);
		}
		break;
	}

	case VAR_TYPE_BYTES:
		xml.writeAttribute("Size", QString::number(Variant.GetSize()));
		xml.writeCharacters (QByteArray((char*)Variant.GetData(), Variant.GetSize()).toBase64());
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

bool QtVariant::Parse(QString &Name, QtVariant &Variant, QXmlStreamReader &xml)
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
			QtVariant Item;
			switch(Type)
			{
			case VAR_TYPE_MAP:
			{
				QtVariant Map;
				while(Parse(Key, Item, xml))
					Map[Key.toStdString().c_str()] = Item;
				Variant = Map;
				return true;
			}
			case VAR_TYPE_INDEX:
			{
				QtVariant Hash;
				while (Parse(Key, Item, xml)) {
					uint32 Index = Key.mid(1).toUInt(NULL, 16);
					Hash[Index] = Item;
				}
				Variant = Hash;
				return true;
			}
			case VAR_TYPE_LIST:
			{
				QtVariant List;
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
				Variant = QtVariant(NULL, 0, VAR_TYPE_EMPTY);
				break;

			case VAR_TYPE_BYTES:
				Variant = QtVariant(QByteArray::fromBase64(Text.toLatin1()));
				break;

			case VAR_TYPE_UNICODE:
				Variant = QtVariant(QString::fromUtf8(QByteArray::fromPercentEncoding(Text.toLatin1())), Type == VAR_TYPE_UTF8);
				break;
			case VAR_TYPE_ASCII:
				Variant = QtVariant(QString::fromLatin1(QByteArray::fromPercentEncoding(Text.toLatin1())));
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
				Variant = QtVariant(Text.toDouble());
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

QString QtVariant::GetTypeStr(int Type)
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

int QtVariant::GetTypeValue(QString Type)
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