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
		
		const hasSheets    = schema.hasOwnProperty('sheets');
        const hasVariables = schema.hasOwnProperty('variables');
        const hasMaps      = schema.hasOwnProperty('maps');

		const hasRootStruct = hasSheets || hasVariables || hasMaps;
		
		if(!hasRootStruct)
		{
			return exportTypes;
		}
		
		const rootStructName = undersoreToPascal(schema.meta.name);
		
		exportTypes.locationMap.push({
			map: `${rootStructName} ${lowerFirstCharacter(rootStructName)} @ 0x00`
		});

        const fields = [];

        const imHexMetaSize = getImHexType(schema.meta.size);

        if(hasMaps)
        {
            const maps = schema.maps;

            maps.forEach( map => {
                exportMap(fields, schema, map, rootStructName, exportTypes);
            });
        }

        if(hasSheets)
        {
		    const sheets = schema.sheets;

            sheets.forEach( sheet => {
                exportSheet(fields, sheet, rootStructName, exportTypes);		
            });
		}
        
        if(hasVariables)
        {
            const variables = schema.variables;
        
            variables.forEach((variable) => {
                const types = variable.types;
            
                const variableName = undersoreToPascal(variable.name);

                const variableType = exportComplexTypes(variableName, types, false, exportTypes);

                fields.push(`${imHexMetaSize} ${variableName}Offset`);
                if(variableType.count > 1)
                {
                    fields.push(`${variableType.type} ${variableName}[${variableType.count}] @ ${variableName}Offset`);
                }
                else
                {
                    fields.push(`${variableType.type} ${variableName} @ ${variableName}Offset`);
                }
            });
        }
        
		exportTypes.structs.push({
			name: rootStructName,
			fields: fields
		});
			
		return exportTypes;
		
        function exportMap(fields, schema, map, rootStructName, exportTypes)
        {   
            const imHexMetaSize = getImHexType(schema.meta.size);

            const mapOffset = `${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(map.type)}Offset`;
            
            const hasMapSheets = map.hasOwnProperty('sheets');
            const hasMapValues = map.hasOwnProperty('variables');

            const hasSheets = schema.hasOwnProperty('sheets');
            const hasValues = schema.hasOwnProperty('variables');

            const mapStructName = `${rootStructName}${undersoreToPascal(map.type)}Map`;
            
            fields.push( ...[`${imHexMetaSize} ${undersoreToPascal(map.type)}Offset`,
                             `${mapStructName} ${undersoreToPascal(map.type)} @ ${undersoreToPascal(map.type)}Offset`]);

            const mapFields = [];
            
            if(hasMapSheets && hasSheets)
            {
                const mapSheets = map.sheets;
                const sheets    = schema.sheets;
      
                mapSheets.forEach( mapSheet => {
                    mapFields.push(...[`${imHexMetaSize} ${undersoreToPascal(mapSheet.target)}CountOffset`,
                        `${imHexMetaSize} ${undersoreToPascal(mapSheet.target)}Count @ ${imHexMetaSize}(${undersoreToPascal(mapSheet.target)}CountOffset + addressof(this))`,
                        `${imHexMetaSize} ${undersoreToPascal(mapSheet.target)}Offset`])
                    
                    let sheet = null;

                    if(mapSheet.hasOwnProperty("source"))
                    {
                        const sheetIndex = sheets.findIndex( s => s.name == mapSheet.source);
                        if(sheetIndex != -1)
                        {
                            sheet = sheets[sheetIndex];
                        }
                        else
                        {
                            console.log(`Unable find sheet ${mapSheet.source} mapping`);
                        }
                    }
                    exportMapSheet(mapFields, sheet, mapSheet, rootStructName, mapStructName, mapOffset, exportTypes);

                });                
            }

            if(hasMapValues && hasValues)
            {
                const mapValues = map.variables;
                const values    = schema.variables;

                mapValues.forEach( mapValue => {
                    
                    mapFields.push(`${imHexMetaSize} ${undersoreToPascal(mapValue.target)}Offset`);
                    
                    if(mapValue.hasOwnProperty("source"))
                    {
                        const valueIndex = values.findIndex( v => v.name == mapValue.source);
                        if(valueIndex != -1)
                        {
                            const variable = values[valueIndex];
                            const types = variable.types;
            
                            const variableType = exportComplexTypes(undersoreToPascal(variable.name), types, false, exportTypes);

                            if(variableType.count > 1)
                            {
                                mapFields.push(`${variableType.type} ${undersoreToPascal(mapValue.target)}[${variableType.count}] @ ${imHexMetaSize}(${undersoreToPascal(mapValue.target)}Offset + addressof(this))`);
                            }
                            else
                            {
                                mapFields.push(`${variableType.type} ${undersoreToPascal(mapValue.target)} @ ${imHexMetaSize}(${undersoreToPascal(mapValue.target)}Offset + addressof(this))`);
                            }
                        }
                        else
                        {
                            console.log(`Unable find variable ${mapValue.source} mapping`);
                        }
                    }
                });
            }

            exportTypes.structs.push({
                name: mapStructName,
                fields: mapFields
            });
        }
        
        function exportMapSheet(mapFields, sheet, mapSheet, rootStructName, mapStructName, mapOffset, exportTypes)
        {
            const mapSheetStructName = `${mapStructName}${undersoreToPascal(mapSheet.target)}`;
            
            const columns = mapSheet.columns;

            const fields = [];

            mapFields.push(`${mapSheetStructName} ${undersoreToPascal(mapSheet.target)} @ ${imHexMetaSize}(${undersoreToPascal(mapSheet.target)}Offset + addressof(this))`);

            columns.forEach( column => {
                fields.push(`${getImHexType(schema.meta.size)} ${undersoreToPascal(column.target)}Offset`);
                
                if(sheet)
                {
                    const sheetStructName = `${rootStructName}${undersoreToPascal(mapSheet.source)}`;
                    
                    let rowCapacity = 0;

                    if(sheet.hasOwnProperty('capacity'))
                    {
                        rowCapacity = resolveExpression(sheet.capacity)|0;
                    }
                    const columnIndex = sheet.columns.findIndex( c => c.name == column.source);

                    if(columnIndex != -1)
                    {
                        const sourceColumn = sheet.columns[columnIndex];

                        const columnStructName = `${sheetStructName}${undersoreToPascal(column.source)}`;
                    
                        let columnType;
			
                        if(sourceColumn.sources.length == 1)
                        {
                            const source = sourceColumn.sources[0];
				
                            if(source.hasOwnProperty('count'))
                            {
                                const count = resolveExpression(source.count)|0;

                                if(count > 1)
                                {
                                    columnType = columnStructName;
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
                            columnType = columnStructName;
                        }


                        fields.push(`${columnType} ${undersoreToPascal(column.target)}[GetCapacity(${rowCapacity}, parent.${undersoreToPascal(mapSheet.target)}Count)] @ ${imHexMetaSize}(${undersoreToPascal(column.target)}Offset + addressof(parent))`);
                    }
                    else
                    {
                        console.log(`Unable find column ${column.source} for sheet ${mapSheet.source} mapping`);
                    }
                }
            });

            exportTypes.structs.push({
				name: mapSheetStructName,
				fields: fields
			});	
        }

		function exportSheet(fields, sheet, rootStructName, exportTypes)
		{
            const imHexMetaSize = getImHexType(schema.meta.size);

			const sheetStructName = `${rootStructName}${undersoreToPascal(sheet.name)}`;

			const columns = sheet.columns;
			
			const sheetName = sheet.name;
			
            let rowCapacity = 0;

			if(sheet.hasOwnProperty('capacity'))
			{
				rowCapacity = resolveExpression(sheet.capacity)|0;
			}
			
            fields.push( ...[`${imHexMetaSize} ${undersoreToPascal(sheet.name)}CountOffset`,
                 `${imHexMetaSize} ${undersoreToPascal(sheet.name)}Count @ ${undersoreToPascal(sheet.name)}CountOffset`,
				 `${imHexMetaSize} ${undersoreToPascal(sheet.name)}Offset`,
                 `${sheetStructName} ${undersoreToPascal(sheet.name)} @ ${undersoreToPascal(sheet.name)}Offset`]);
			
            const sheetFields = [];

			columns.forEach( column => {
				exportColumn(sheetFields, column, rootStructName, rowCapacity, sheetName, exportTypes);
			});

			exportTypes.structs.push({
				name: sheetStructName,
				fields: sheetFields
			});
		}
		
		function exportColumn(fields, column, rootStructName, rowCapacity, sheetName, exportTypes)
		{
			const sources = column.sources;
			
			const sheetStructName  = `${rootStructName}${undersoreToPascal(sheetName)}`;
			const columnStructName = `${sheetStructName}${undersoreToPascal(column.name)}`;
			
            const columnType = exportComplexTypes(columnStructName, sources, true, exportTypes);

            fields.push(`${imHexMetaSize} ${undersoreToPascal(column.name)}Offset`);
            fields.push(`${columnType.type} ${undersoreToPascal(column.name)}[GetCapacity(${rowCapacity}, parent.${undersoreToPascal(sheetName)}Count)] @ ${undersoreToPascal(column.name)}Offset`);
		}
	}
	
    function exportComplexTypes(name, types, alwaysWrapInStruct, exportTypes)
    {
        let exportedType;
		let exportedCount = 1;	
        let exportStruct = false;

        if(types.length == 1)
        {
            const type = types[0];
				
            if(type.hasOwnProperty('count'))
            {
                exportedCount = resolveExpression(type.count)|0;

                if(exportedCount > 1)
                {
                    exportStruct = alwaysWrapInStruct;
                }
            }
            exportedType = getImHexType(type.type);
        }
        else
        {
            exportStruct = true;
        }
			
        if(exportStruct)
        {
            const fields = [];

            types.forEach( type => {
                let field = '';
                const typeName = undersoreToPascal(type.name);
                const typeType = getImHexType(type.type);
                if(type.hasOwnProperty('count'))
                {
                    const count = resolveExpression(type.count)|0;
                    if(count > 1)
                    {
                        field = `${typeType} ${typeName}[${count}]`;
                    }
                    else
                    {
                        field = `${typeType} ${typeName}`;
                    }
                }
                else
                {
                    field = `${typeType} ${typeName}`;
                }
                fields.push(field);
            });

            exportTypes.structs.push({
                name: name,
                fields: fields
            });
				
            exportedType = name;
            exportedCount = 1;
        }

        return {
            type: exportedType,
            count: exportedCount
        };
    }

	function exportTypesToText(exportTypes)
	{
        const imHexMetaSize = getImHexType(schema.meta.size);

		let text = `fn GetCapacity(${imHexMetaSize} capacity, ${imHexMetaSize} count)\n`;
		text += '{\n';
		text += `  ${imHexMetaSize} result = capacity;\n`;
		text += '  if(result == 0) {\n';
		text += '    result = count;\n';
		text += '  }\n';
		text += '  return result;\n';
		text += '};\n';
		
        text += '\n';

		exportTypes.structs.forEach((struct) => {
			text += `using ${struct.name};\n`
		});
		
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