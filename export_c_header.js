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

const exportText = buildCHeader(schema);

fs.writeFileSync(outputFile, exportText, 'utf8')

function buildCHeader(schema)
{
	const exportTypes = exportSheets();
	
	const exportText = exportTypesToText(exportTypes);
	
	return exportText;
	
	function exportSheets()
	{
		const exportTypes = {
			constants:   [],
			packStructs: [],
            structs:     [],
			functions:   [],
            refStructs:  [],
		};
		
		const hasSheets    = schema.hasOwnProperty('sheets');
        const hasVariables = schema.hasOwnProperty('variables');
        const hasMaps      = schema.hasOwnProperty('maps');

		const hasRootStruct = hasSheets || hasVariables || hasMaps;
		
        const rootStructName = undersoreToPascal(schema.meta.name);

		if(schema.hasOwnProperty('constants'))
		{
			const constants = schema.constants;
			
			constants.forEach((c) => {
				exportTypes.constants.push({
					key: `${rootStructName}${undersoreToPascal(c.name)}`,
					value: c.value
				});
			});
			
		}
		
        const fields = [];

        if(hasMaps)
        {
            const maps = schema.maps;

            maps.forEach( map => {
                const mapType = undersoreToPascal(map.type);
                fields.push(`${schema.meta.size} ${mapType}MapOffset`);

            	exportTypes.functions.push({
				    declaration: `${mapType} *${rootStructName}${mapType}MapPrt(${rootStructName} *root)`,
				    body: `return (${mapType} *)((uintptr_t)root + root->${mapType}MapOffset);`
			    });
                
                exportTypes.refStructs.push(mapType);
            });
        }

        if(hasSheets)
        {
		    const sheets = schema.sheets;
            fields.push( ...sheets.flatMap((sheet) => 
				[`${schema.meta.size} ${undersoreToPascal(sheet.name)}CountOffset`,
				 `${schema.meta.size} ${undersoreToPascal(sheet.name)}Offset`]) 
            );

            sheets.forEach( sheet => {
            	exportTypes.functions.push({
				    declaration: `${schema.meta.size} *${rootStructName}${undersoreToPascal(sheet.name)}CountPrt(${rootStructName} *root)`,
				    body: `return (${schema.meta.size} *)((uintptr_t)root + root->${undersoreToPascal(sheet.name)}CountOffset);`
			    });
                exportSheet(sheet, rootStructName, exportTypes);		
            });
        }

        if(hasVariables)
        {
            const variables = schema.variables;
        
            variables.forEach( variable => {
                fields.push(`${schema.meta.size} ${undersoreToPascal(variable.name)}Offset`);
                exportVariable(variable, exportTypes);
            });
        }

        if(hasRootStruct)
        {
            exportTypes.packStructs.push({
                name: rootStructName,
                fields: fields
            });
        }	   
        
        if(schema.hasOwnProperty('ref'))
        {
            const contextStructName = `${rootStructName}Context`

            const contextStruct = {
                name: contextStructName,
                fields: [],
            };

            if(hasRootStruct)
            {
                contextStruct.fields.push(`${rootStructName} *Root`);
            }

            contextStruct.fields.push( ...schema.ref.map(ref => {
                const refName = undersoreToPascal(ref.name);
                const refType = undersoreToPascal(ref.type);

                return `${refType} *${refName}`;
            }));

            exportTypes.refStructs.push( ...schema.ref.map(ref => {
                const refType = undersoreToPascal(ref.type);

                return refType;
            }));

            exportTypes.structs.push(contextStruct);
        }

		return exportTypes;
		
        function exportVariable(variable, exportTypes)
        {
            const types = variable.types;
            
            const variableName = undersoreToPascal(variable.name);

            const variableType = exportComplexTypes(`${rootStructName}${undersoreToPascal(variable.name)}`, types, exportTypes);

            exportTypes.functions.push({
				declaration: `${variableType.type} *${rootStructName}${variableName}Prt(${rootStructName} *root)`,
				body: `return (${variableType.type} *)((uintptr_t)root + root->${variableName}Offset);`
			});
        }

		function exportSheet(sheet, rootStructName, exportTypes)
		{
			const sheetStructName = `${rootStructName}${undersoreToPascal(sheet.name)}`;
			
			const columns = sheet.columns;
			
			const sheetName = sheet.name;
			
			exportTypes.packStructs.push({
				name: sheetStructName,
				fields: columns.flatMap((column) => [ 
					`${schema.meta.size} ${undersoreToPascal(column.name)}Offset` ])
			});
			
			exportTypes.functions.push({
				declaration: `${sheetStructName} *${sheetStructName}Prt(${rootStructName} *root)`,
				body: `return (${sheetStructName} *)((uintptr_t)root + root->${undersoreToPascal(sheetName)}Offset);`
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
						
            const columnType = exportComplexTypes(columnStructName, sources, exportTypes);

			exportTypes.functions.push({
				declaration: `${columnType.type} *${columnStructName}Prt(${rootStructName} *root, ${sheetStructName} *sheet)`,
				body: `return (${columnType.type} *)((uintptr_t)root + sheet->${undersoreToPascal(column.name)}Offset);`
			});
			
		}		
	}
	
    function exportComplexTypes(name, types, exportTypes)
    {
        let exportedType = '';
        let exportStruct = false;
        let exportedCount = 1;

        if(types.length == 1)
        {
            type = types[0];
            if(type.hasOwnProperty('count'))
            {
                exportedCount = resolveExpression(type.count)|0;

                if(exportedCount > 1)
                {
                    exportStruct = true;    
                }
            }
            exportedType = type.type;
        }
        else
        {
            exportStruct = true;
        }

        if(exportStruct)
        {
            exportedType = name;
            exportedCount = 1;

            const fields = [];

            types.forEach((t) => {
                let field = '';
                if(t.hasOwnProperty('count'))
                {
                    const count = resolveExpression(t.count)|0;
                    if(count > 1)
                    {
                        field = `${t.type} ${undersoreToPascal(t.name)}[${count}]`;
                    }
                    else
                    {
                        field = `${t.type} ${undersoreToPascal(t.name)}`;
                    }
                }
                else
                {
                    field = `${t.type} ${undersoreToPascal(t.name)}`;
                }
                fields.push(field);
            });

            exportTypes.packStructs.push({
                name: exportedType,
                fields: fields
            });                    
        }

        return {
            type: exportedType,
            count: exportedCount
        }
    }

	function exportTypesToText(exportTypes)
	{
		let text = '#pragma once\n';
		text += '\n';
		
		text += '#include <stdint.h>\n';
		text += '\n';
		
        if(exportTypes.packStructs.length > 0 || exportTypes.structs.length > 0 || exportTypes.refStructs.length > 0)
        {
            text += '#ifndef __cplusplus\n';
            exportTypes.packStructs.forEach((struct) => {
                text += `typedef struct ${struct.name.padEnd(40, ' ')} ${struct.name};\n`;
            });
            exportTypes.structs.forEach((struct) => {
                text += `typedef struct ${struct.name.padEnd(40, ' ')} ${struct.name};\n`;
            });
            exportTypes.refStructs.forEach((struct) => {
                text += `typedef struct ${struct.padEnd(40, ' ')} ${struct};\n`;
            });		
            text += '#endif\n';
		    text += '\n';
        }
		
        if(exportTypes.constants.length > 0)
        {
            exportTypes.constants.forEach((c) => {
                text += `#define k${c.key.padEnd(40, ' ')} ${c.value}\n`;
            });

            text += '\n';
        }

        if(exportTypes.packStructs.length > 0)
        {
            text += '#pragma pack(push, 1)';
            text += '\n';

            exportTypes.packStructs.forEach((struct) => {
                text += `struct ${struct.name}`
                text += '\n';
                text += '{';
                text += '\n';
                struct.fields.forEach((field) => {
                    text += `  ${field};`;
                    text += '\n';
                });
                text += '};'
                text += '\n';
            });
            text += '#pragma pack(pop)';
            text += '\n';
            text += '\n';
        }		
		
        if(exportTypes.structs.length > 0)
        {
            exportTypes.structs.forEach((struct) => {
                text += `struct ${struct.name}`
                text += '\n';
                text += '{';
                text += '\n';
                struct.fields.forEach((field) => {
                    text += `  ${field};`;
                    text += '\n';
                });
                text += '};'
                text += '\n';
            });
        
            text += '\n';
        }

		exportTypes.functions.forEach((fun) => {
			text += `static inline`
			text += '\n';
			text += fun.declaration;
			text += '\n';
			text += '{';
			text += '\n';
			text += `  ${fun.body}`;
			text += '\n';
			text += '}'
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