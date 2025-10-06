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

const importedSchemas = {};
if(schema.meta.hasOwnProperty('import'))
{
    schema.meta.import.forEach( importName => {
        const importSchemaFile = `${importName}.schema.yml`;
        const importSchema = yaml.load(fs.readFileSync(importSchemaFile), 'utf8');

        const importedVMContext = {};
        if(importSchema.hasOwnProperty('constants'))
        {
            const constants = importSchema.constants;
            constants.forEach( constant => {
                importedVMContext[constant.name] = constant.value;
            });
        }

        importedSchemas[importName] = {
            schema: importSchema,
            context: importedVMContext
        };
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

                const variableType = exportComplexTypes(vmContext, variableName, types, false, exportTypes);

                fields.push(`${imHexMetaSize} ${variableName}Offset`);
                if(variableType.count > 1)
                {
                    fields.push(`if(${variableName}Offset > 0) ${variableType.type} ${variableName}[${variableType.count}] @ ${variableName}Offset`);
                }
                else
                {
                    fields.push(`if(${variableName}Offset > 0) ${variableType.type} ${variableName} @ ${variableName}Offset`);
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

            const mapSegmentType = map.type;
            let mapSegmentName = map.type;

            if(map.hasOwnProperty('name'))
            {
                mapSegmentName = map.name;
            }

            if(!importedSchemas.hasOwnProperty(mapSegmentType))
            {
                Log(`missing import for map type ${mapSegmentName}`);
                return;
            }

            const targetSchema = importedSchemas[mapSegmentType].schema;
            const targetContext = importedSchemas[mapSegmentType].context;

            const mapOffset = `${lowerFirstCharacter(rootStructName)}.${undersoreToPascal(mapSegmentName)}Offset`;
            
            const targetHasSheets = targetSchema.hasOwnProperty('sheets');
            const targetHasValues = targetSchema.hasOwnProperty('variables');

            const hasMapSheets = map.hasOwnProperty('sheets');
            const hasMapValues = map.hasOwnProperty('variables');

            const mapStructName = `${rootStructName}${undersoreToPascal(mapSegmentName)}Map`;
            
            fields.push( ...[`${imHexMetaSize} ${undersoreToPascal(mapSegmentName)}Offset`,
                             `if(${undersoreToPascal(mapSegmentName)}Offset > 0) ${mapStructName} ${undersoreToPascal(mapSegmentName)} @ ${undersoreToPascal(mapSegmentName)}Offset`]);

            const mapFields = [];
            
            if(targetHasSheets)
            {
                const targetSheets = targetSchema.sheets;
      
                targetSheets.forEach( targetSheet => {
                    mapFields.push(...[`${imHexMetaSize} ${undersoreToPascal(targetSheet.name)}CountOffset`,
                        `${imHexMetaSize} ${undersoreToPascal(targetSheet.name)}Count @ ${imHexMetaSize}(${undersoreToPascal(targetSheet.name)}CountOffset + addressof(this))`,
                        `${imHexMetaSize} ${undersoreToPascal(targetSheet.name)}Offset`])
                    
                    let mapSourceSheet = null;
                    if(hasMapSheets)
                    {
                        const mapSheetIndex = map.sheets.findIndex( s => s.target == targetSheet.name);
                        if(mapSheetIndex != -1)
                        {
                            mapSourceSheet = map.sheets[mapSheetIndex];
                        }
                    }

                    exportMapSheet(targetContext, mapFields, mapSourceSheet, targetSheet, rootStructName, mapStructName, mapOffset, exportTypes);

                });                
            }

            if(targetHasValues)
            {
                const targetValues = targetSchema.variables;

                targetValues.forEach( targetValue => {
                    
                    mapFields.push(`${imHexMetaSize} ${undersoreToPascal(targetValue.name)}Offset`);
                    
                    let variableContext = targetContext;
                    let variableTypeDef = targetValue.types;

                    if(hasMapValues)
                    {
                        const mapValueIndex = map.variables.findIndex( v => v.target == targetValue.name);
                        if(mapValueIndex != -1)
                        {
                            const mapValue = map.variables[mapValueIndex];

                            if(!mapValue.hasOwnProperty("source"))
                            {
                                Log(`source value for ${targetValue} in map ${mapSegmentName} not specify`);                                
                            }
                            else
                            {
                                const sourceVariable = mapValue.source;

                                const hasVariables = schema.hasOwnProperty('variables');
                                if(hasVariables)
                                {
                                    const sourceVariables = schema.variables;
                                    const sourceValueIndex = sourceVariables.findIndex( v => v.name == sourceVariable);
                                    if(sourceValueIndex != -1)
                                    {
                                        const sourceVariable = sourceVariables[sourceValueIndex];
                                        variableContext = vmContext;
                                        variableTypeDef = sourceVariable.types;
                                    }
                                    else
                                    {
                                        Log(`source variable ${sourceVariable} is not define is schema`);
                                    }
                                }
                                else
                                {
                                    Log(`source variable ${sourceVariable} is not define is schema`);
                                }
                            }
                        }
                    }

                    const variableType = exportComplexTypes(variableContext, undersoreToPascal(targetValue.name), variableTypeDef, false, exportTypes);

                    if(variableType.count > 1)
                    {
                        mapFields.push(`if(${undersoreToPascal(targetValue.name)}Offset > 0) ${variableType.type} ${undersoreToPascal(targetValue.name)}[${variableType.count}] @ ${imHexMetaSize}(${undersoreToPascal(targetValue.name)}Offset + addressof(this))`);
                    }
                    else
                    {
                        mapFields.push(`if(${undersoreToPascal(targetValue.name)}Offset > 0) ${variableType.type} ${undersoreToPascal(targetValue.name)} @ ${imHexMetaSize}(${undersoreToPascal(targetValue.name)}Offset + addressof(this))`);
                    }
                });
            }

            exportTypes.structs.push({
                name: mapStructName,
                fields: mapFields
            });
        }
        
        function exportMapSheet(targetContext, mapFields, mapSourceSheet, targetSheet, rootStructName, mapStructName, mapOffset, exportTypes)
        {
            let rowCapacity = 0;

            if(targetSheet.hasOwnProperty('capacity'))
            {
                rowCapacity = resolveExpression(targetContext, targetSheet.capacity)|0;
            }

            const mapSheetStructName = `${mapStructName}${undersoreToPascal(targetSheet.name)}`;
            
            const targetColumns = targetSheet.columns;

            const fields = [];

            mapFields.push(`if(${undersoreToPascal(targetSheet.name)}Offset > 0) ${mapSheetStructName} ${undersoreToPascal(targetSheet.name)} @ ${imHexMetaSize}(${undersoreToPascal(targetSheet.name)}Offset + addressof(this))`);

            targetColumns.forEach( targetColumn => {
                fields.push(`${getImHexType(schema.meta.size)} ${undersoreToPascal(targetColumn.name)}Offset`);
                
                const sheetStructName = `${rootStructName}${undersoreToPascal(targetSheet.name)}`;
                                    
                let columnSources = targetColumn.sources;
                let columnContext = targetContext;

                if(mapSourceSheet)
                {
                    const mapSourceSheetColumns = mapSourceSheet.columns;
                    let sourceSheetName = mapSourceSheet.source;

                    const mapSourceSheetColumnIndex = mapSourceSheetColumns.findIndex( v => v.target == targetColumn.name);
                    if(mapSourceSheetColumnIndex != -1)
                    {
                        const mapSourceSheetColumn = mapSourceSheetColumns[mapSourceSheetColumnIndex];

                        let sourceSheet = null;

                        if(mapSourceSheetColumn.hasOwnProperty('sheet'))
                        {
                            sourceSheetName = mapSourceSheetColumn.sheet;
                        }

                        const hasSourceSheets = schema.hasOwnProperty('sheets');
                        if(hasSourceSheets)
                        {
                            const sourceSheets = schema.sheets;
                    
                            const sourceSheetIndex = sourceSheets.findIndex( v => v.name == sourceSheetName);
                            if(sourceSheetIndex != -1)
                            {
                                sourceSheet = sourceSheets[sourceSheetIndex];
                        
                                if(sourceSheet.hasOwnProperty('capacity'))
                                {
                                    rowCapacity = resolveExpression(vmContext, sourceSheet.capacity)|0;
                                }
                            }
                            else
                            {
                                Log(`source sheet ${sourceSheetName} is not define is schema`);
                            }
                        }
                        else
                        {
                            Log(`source sheet ${sourceSheetName} is not define is schema`);
                        }
                        
                        if(mapSourceSheetColumn.hasOwnProperty('source'))
                        {
                            const sourceSheetColumnName = mapSourceSheetColumn.source;
                            if(sourceSheet)
                            {
                                const sourceColumns = sourceSheet.columns;

                                const sourceSheetColumnIndex = sourceColumns.findIndex( v => v.name == sourceSheetColumnName);
                                if(sourceSheetColumnIndex != -1)
                                {
                                    const sourceSheetColumn = sourceColumns[sourceSheetColumnIndex];
                                    
                                    columnSources = sourceSheetColumn.sources;
                                    columnContext = vmContext;
                                }
                                else
                                {
                                    Log(`source sheet ${sourceSheetName} is not define is schema`);
                                }
                            }
                            else
                            {
                                Log(`source sheet not found`);                                
                            }
                        }
                        else
                        {
                            Log(`source column for ${targetColumn.name} in map ${mapSheetStructName} not specify`);
                        }
                    }
                }

                const columnStructName = `${sheetStructName}${undersoreToPascal(targetColumn.name)}`;
                const columnType = exportComplexTypes(columnContext, columnStructName, columnSources, true, exportTypes);

                fields.push(`if(${undersoreToPascal(targetColumn.name)}Offset > 0) ${columnType.type} ${undersoreToPascal(targetColumn.name)}[GetCapacity(${rowCapacity}, parent.${undersoreToPascal(targetSheet.name)}Count)] @ ${imHexMetaSize}(${undersoreToPascal(targetColumn.name)}Offset + addressof(parent))`);
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
				rowCapacity = resolveExpression(vmContext, sheet.capacity)|0;
			}
			
            fields.push( ...[`${imHexMetaSize} ${undersoreToPascal(sheet.name)}CountOffset`,
                 `${imHexMetaSize} ${undersoreToPascal(sheet.name)}Count @ ${undersoreToPascal(sheet.name)}CountOffset`,
				 `${imHexMetaSize} ${undersoreToPascal(sheet.name)}Offset`,
                 `if(${undersoreToPascal(sheet.name)}Offset > 0) ${sheetStructName} ${undersoreToPascal(sheet.name)} @ ${undersoreToPascal(sheet.name)}Offset`]);
			
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
			
            const columnType = exportComplexTypes(vmContext, columnStructName, sources, true, exportTypes);

            fields.push(`${imHexMetaSize} ${undersoreToPascal(column.name)}Offset`);
            fields.push(`if(${undersoreToPascal(column.name)}Offset > 0) ${columnType.type} ${undersoreToPascal(column.name)}[GetCapacity(${rowCapacity}, parent.${undersoreToPascal(sheetName)}Count)] @ ${undersoreToPascal(column.name)}Offset`);
		}
	}
	
    function exportComplexTypes(currentContext, name, types, alwaysWrapInStruct, exportTypes)
    {
        let exportedType;
		let exportedCount = 1;	
        let exportStruct = false;

        if(types.length == 1)
        {
            const type = types[0];
				
            if(type.hasOwnProperty('count'))
            {
                exportedCount = resolveExpression(currentContext, type.count)|0;

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
                    const count = resolveExpression(currentContext, type.count)|0;
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

function findSheetByName(sheets, sheetName)
{
    let sheet = null;
    const sheetIndex = sheets.findIndex( s => s.name == sheetName);
    if(sheetIndex != -1)
    {
        sheet = sheets[sheetIndex];
    }
    else
    {
        Log(`Unable find sheet ${sheetName} mapping`);
    }

    return sheet;
}

function resolveExpression(currentContext, text)
{
	const code = `_result = ${text};`;
	const context = { ...currentContext };
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

function Log(text)
{
    console.log(`${text} | ${schemaFile}`)
}