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

        const hasVariables = schema.hasOwnProperty('variables');

		const hasRootStruct = hasSheets || hasVariables;
		
		if(!hasRootStruct)
		{
			return exportTypes;
		}
		
		const rootStructName = undersoreToPascal(schema.meta.name);
		
		exportTypes.locationMap.push({
			map: `${rootStructName} ${lowerFirstCharacter(rootStructName)} @ 0x00`
		});

        const fields = [];

        if(hasSheets)
        {
		    const sheets = schema.sheets;
            fields.push( ...sheets.flatMap((sheet) => 
				[`${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}CountOffset`,
                 `${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}Count @ ${undersoreToPascal(sheet.name)}CountOffset`,
				 `${getImHexType(schema.meta.size)} ${undersoreToPascal(sheet.name)}Offset`]) 
            );

            sheets.forEach( sheet => {
                exportSheet(sheet, rootStructName, exportTypes);		
            });
		}
        
        if(hasVariables)
        {
            const variables = schema.variables;
        
            variables.forEach((variable) => {
                const types = variable.types;
            
                let variableType = '';
                let exportStruct = false;
                let variableCount = 1;

                if(types.length == 1)
                {
                    type = types[0];
                    if(type.hasOwnProperty('count'))
                    {
                        variableCount = resolveExpression(type.count)|0;
                    }
                    variableType = getImHexType(type.type);
                }
                else
                {
                    exportStruct = true;
                }

                if(exportStruct)
                {
                    variableType = undersoreToPascal(variable.name);

                    const valuableFields = [];

                    types.forEach((t) => {
                        let field = '';
                        if(t.hasOwnProperty('count'))
                        {
                            const count = resolveExpression(t.count)|0;
                            if(count > 1)
                            {
                                field = `${getImHexType(t.type)} ${undersoreToPascal(t.name)}[${count}]`;
                            }
                            else
                            {
                                field = `${getImHexType(t.type)} ${undersoreToPascal(t.name)}`;
                            }
                        }
                        else
                        {
                            field = `${getImHexType(t.type)} ${undersoreToPascal(t.name)}`;
                        }
                        valuableFields.push(field);
                    });

                    exportTypes.structs.push({
                        name: variableType,
                        fields: valuableFields
                    });                    
                }

                if(variableCount > 1)
                {
                    fields.push(`${variableType} ${undersoreToPascal(variable.name)}[${variableCount}]`);
                }
                else
                {
                    fields.push(`${variableType} ${undersoreToPascal(variable.name)}`);
                }
            });
        }
		exportTypes.structs.push({
			name: rootStructName,
			fields: fields
		});
			
		return exportTypes;
		
		function exportSheet(sheet, rootStructName, exportTypes)
		{
			const sheetStructName = `${rootStructName}${undersoreToPascal(sheet.name)}`;
			
			const columns = sheet.columns;
			
			const sheetName = sheet.name;
			
            let rowCapacity = 0;

			if(sheet.hasOwnProperty('capacity'))
			{
				rowCapacity = resolveExpression(sheet.capacity)|0;
			}

			exportTypes.structs.push({
				name: sheetStructName,
				fields: columns.flatMap((column) => [ 
					`${getImHexType(schema.meta.size)} ${undersoreToPascal(column.name)}Offset` ])
			});
			
			exportTypes.locationMap.push({
				map: `${sheetStructName} ${lowerFirstCharacter(sheetStructName)} @ ${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(sheetName)}Offset`
			});
			
			columns.forEach( column => {
				exportColumn(column, rootStructName, rowCapacity, sheetName, exportTypes);
			});
		}
		
		function exportColumn(column, rootStructName, rowCapacity, sheetName, exportTypes)
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
					const count = resolveExpression(source.count)|0;

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
                        const count = resolveExpression(source.count)|0;
						if(count > 1)
						{
							field = `${getImHexType(source.type)} ${undersoreToPascal(source.name)}[${count}]`;
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
				map: `${columnType} ${lowerFirstCharacter(columnStructName)}[GetCapacity(${rowCapacity}, ${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(sheetName)}Count)] @ ${lowerFirstCharacter(sheetStructName)}.${undersoreToPascal(column.name)}Offset`
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
				'float'   : 'float',
                'uint64_t': 'u64',
                'int64_t' : 's64',
                'double'  : 'double',
			}				
			return typeMap[type];
		}
	}
	
	function exportTypesToText(exportTypes)
	{
		let text = 'fn GetCapacity(u16 capacity, u16 count)\n';
		text += '{\n';
		text += '  u16 result = capacity;\n';
		text += '  if(result == 0) {\n';
		text += '    result = count;\n';
		text += '  }\n';
		text += '  return result;\n';
		text += '};\n';
		
        text += '\n';

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
	const code = `_result = ${text};`;
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