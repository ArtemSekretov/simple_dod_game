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
			structs:     [],
			functions:   []
		};
		
		const hasSheets = schema.hasOwnProperty('sheets');

		const hasRootStruct = hasSheets;
		
		if(!hasRootStruct)
		{
			return exportTypes;
		}
		
		if(schema.hasOwnProperty('constants'))
		{
			const constants = schema.constants;
			
			constants.forEach((c) => {
				exportTypes.constants.push({
					key: undersoreToPascal(c.name),
					value: c.value
				});
			});
			
		}
		const rootStructName = undersoreToPascal(schema.meta.name);
		
		const sheets = schema.sheets;
		
		exportTypes.structs.push({
			name: rootStructName,
			fields: sheets.flatMap((sheet) => 
				[`${schema.meta.size} ${undersoreToPascal(sheet.name)}Count`,
				 `${schema.meta.size} ${undersoreToPascal(sheet.name)}Capacity`,
				 `${schema.meta.size} ${undersoreToPascal(sheet.name)}Offset`])
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
						columnType = source.type;
					}
				}
				else
				{
					columnType = source.type;
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
                            field = `${source.type} ${undersoreToPascal(source.name)}[${source.count}]`;
                        }
                        else
                        {
                            field = `${source.type} ${undersoreToPascal(source.name)}`;
                        }
                    }
                    else
                    {
                        field = `${source.type} ${undersoreToPascal(source.name)}`;
                    }
                    fields.push(field);
                });

                exportTypes.structs.push({
                name: columnStructName,
                fields: fields
                });
        
                columnType = columnStructName;
            }
			
			exportTypes.functions.push({
				declaration: `${columnType} *${columnStructName}Prt(${rootStructName} *root, ${sheetStructName} *sheet)`,
				body: `return (${columnType} *)((uintptr_t)root + sheet->${undersoreToPascal(column.name)}Offset);`
			});
			
		}		
	}
	
	function exportTypesToText(exportTypes)
	{
		let text = '#pragma once\n';
		text += '\n';
		
		text += '#include <stdint.h>\n';
		text += '\n';
		
		text += '#ifndef __cplusplus\n';
		exportTypes.structs.forEach((struct) => {
			text += `typedef struct ${struct.name.padEnd(40, ' ')} ${struct.name};\n`;
		});		
		text += '#endif\n';
		text += '\n';
		
		exportTypes.constants.forEach((c) => {
			text += `#define k${c.key.padEnd(40, ' ')} ${c.value}\n`;
		});
		
		text += '\n';
		
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