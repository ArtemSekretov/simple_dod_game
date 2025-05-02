const yaml = require('js-yaml');
const fs   = require('fs');
const vm   = require('vm');

const vmContext = {};

const schemaFile = process.argv[2];
const outputFile = process.argv[3];

const schema = yaml.load(fs.readFileSync(schemaFile), 'utf8');

if(schema.hasOwnProperty('constants'))
{
	const constants = schema.constants;
	constants.forEach( constant => {
		vmContext[constant.name] = constant.value;
	});
}

const exportText = buildImHexPattern(schema);

fs.writeFileSync(outputFile, exportText, 'utf8')

function buildImHexPattern(schema)
{
	const exportTypes = exportSheets();
	
	const exportText = exportTypesToText(exportTypes);
	
	return exportText;
	
	function exportSheets()
	{
		const exportTypes = {
			structs:     [],
			locationMap: []
		};
		
		const hasSheets = schema.hasOwnProperty('sheets');

		const hasRootStruct = hasSheets;
		
		if(!hasRootStruct)
		{
			return exportTypes;
		}
		
		const rootStructName = undersoreToPascal(schema.meta.name);
		
		const sheets = schema.sheets;
		
		exportTypes.structs.push({
			name: rootStructName,
			fields: sheets.flatMap((sheet) => 
				[`${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}Count`,
				 `${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}Capacity`,
				 `${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}Offset`])
		});
		
		exportTypes.locationMap.push({
			map: `${rootStructName} ${lowerFirstCharacter(rootStructName)} @ 0x00`
		});
		
		sheets.forEach( sheet => {
			exportSheet(sheet, rootStructName, exportTypes);		
		});
		
		return exportTypes;
		
		function exportSheet(sheet, rootStructName, exportTypes)
		{
			const sheetStructName = `${rootStructName}${undersoreToPascal(sheet.name)}`;
			
			const columns = sheet.columns;
			
			const sheetName = sheet.name;
			
			exportTypes.structs.push({
				name: sheetStructName,
				fields: columns.flatMap((column) => [ 
					`${getImHexType(schema.meta.size)} ${undersoreToPascal(column.name)}Offset` ])
			});
			
			exportTypes.locationMap.push({
				map: `${sheetStructName} ${lowerFirstCharacter(sheetStructName)} @ ${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(sheetName)}Offset`
			});
			
			columns.forEach( column => {
				exportColumn(column, rootStructName, sheetName, exportTypes);
			});
		}
		
		function exportColumn(column, rootStructName, sheetName, exportTypes)
		{
			const sources = column.sources;
			
			const sheetStructName  = `${rootStructName}${undersoreToPascal(sheetName)}`;
			const columnStructName = `${sheetStructName}${undersoreToPascal(column.name)}`;
			
			let columnType;
			
			let exportStruct = false;

			if(sources.length == 1)
			{
				const source = sources[0];
				
				if(source.hasOwnProperty('count'))
				{
					const count = source.count;

					if(count > 1)
					{
						exportStruct = true;
					}
					else
					{
						columnType = getImHexType(source.type);
					}
				}
				else
				{
					columnType = getImHexType(source.type);
				}
			}
			else
			{
				exportStruct = true;
			}
			
			if(exportStruct)
			{
				const fields = [];

				sources.forEach((source) => {
					let field = '';
					if(source.hasOwnProperty('count'))
					{
						if(source.count > 1)
						{
							field = `${getImHexType(source.type)} ${undersoreToPascal(source.name)}[${source.count}]`;
						}
						else
						{
							field = `${getImHexType(source.type)} ${undersoreToPascal(source.name)}`;
						}
					}
					else
					{
						field = `${getImHexType(source.type)} ${undersoreToPascal(source.name)}`;
					}
					fields.push(field);
				});

				exportTypes.structs.push({
					name: columnStructName,
					fields: fields
				});
				
				columnType = columnStructName;
			}
			exportTypes.locationMap.push({
				map: `${columnType} ${lowerFirstCharacter(columnStructName)}[${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(sheetName)}Capacity] @ ${lowerFirstCharacter(sheetStructName)}.${undersoreToPascal(column.name)}Offset`
			});	
			
		}
		
		function getImHexType(type)
		{
			const typeMap = {
				'uint8_t' : 'u8',
				'int8_t'  : 's8',
				'uint16_t': 'u16',
				'int16_t' : 's16',
				'uint32_t': 'u32',
				'int32_t' : 's32',
				'float'   : 'float'
			}				
			return typeMap[type];
		}
	}
	
	function exportTypesToText(exportTypes)
	{
		let text = '';
		
		exportTypes.structs.forEach((struct) => {
			text += `struct ${struct.name} {`
			text += '\n';
			struct.fields.forEach((field) => {
				text += `  ${field};`;
				text += '\n';
			});
			text += '};'
			text += '\n';
			text += '\n';
		});
		
		text += '\n';
		
		exportTypes.locationMap.forEach((map) => {
			text += map.map;
			text += ';';
			text += '\n';
		});
				
		
		return text;
	}
}

function resolveExpression(text)
{
	const code = '_result = ${text};';
	const context = { ...vmContext };
	vm.runInNewContext(code, context);
	return context['_result'];
}

function undersoreToPascal(text)
{
	const words = text.split('_');
	const capitalizedWords = words.map( word => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase() );
	return capitalizedWords.join('');
}

function lowerFirstCharacter(text)
{
	const result = text.charAt(0).toLowerCase() + text.slice(1);
	return result;
}