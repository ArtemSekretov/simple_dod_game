const yaml = require('js-yaml');
const fs   = require('fs');
const vm   = require('vm');
const XLSX = require('xlsx');

const vmContext = {};

const schemaFile = process.argv[2];
const outputFile = process.argv[3];
const sheetFile  = process.argv[4];

const schema = yaml.load(fs.readFileSync(schemaFile), 'utf8');

if(schema.hasOwnProperty('constants'))
{
	const constants = schema.constants;
	constants.forEach( constant => {
		vmContext[constant.name] = constant.value;
	});
}

let sourceWorkbook = null;
if(sheetFile)
{
	sourceWorkbook = XLSX.read( new Uint8Array(fs.readFileSync(sheetFile)).buffer);
}

const data = buildRuntimeBinary(schema, sourceWorkbook);

fs.writeFileSync(outputFile, Buffer.from(data), 'binary')

function buildRuntimeBinary(schema, sourceWorkbook)
{
	const exportDataSegmentOffsets = [];
    const relocationTable = [];

	let data = exportSheets();
	
    relocationTable.forEach( relocation => {
        let offset = relocation.offset;
        const size = relocation.size;

        relocation.names.forEach( name => {
            const segmentOffset = exportDataSegmentOffsets[name]|0;
            
            const bytes = bytesAsSize([segmentOffset], size);

            for(let i = 0; i < bytes.length; i++)
            {
                data[offset + i] = bytes[i];
            }

            offset += bytes.length;
        });
    });

	return data;

	function exportSheets()
	{
		const data = [];
        const exportDataSegments = [];

		const hasSheets = schema.hasOwnProperty('sheets');
        const hasVariables = schema.hasOwnProperty('variables');

		const hasRootStruct = hasSheets || hasVariables;
		
		if(!hasRootStruct)
		{
			return data;
		}

		if(hasSheets)
		{
			const sheets = schema.sheets;

			sheets.forEach( sheet => {
				
				let sourceWorksheet = null;
				if(sourceWorkbook)
				{
					const sourceWorksheetName = undersoreToPascal(sheet.name);
					if(sourceWorkbook.SheetNames.indexOf(sourceWorksheetName) == -1)
					{
						console.log(`Sheet ${sourceWorksheetName} not found`);
					}
					else
					{
						sourceWorksheet = sourceWorkbook.Sheets[sourceWorksheetName];
					}
				}
				
				exportSheet(data, sheet, sourceWorksheet, exportDataSegments);		
			});
		}

        if(hasVariables)
        {
            const variables = schema.variables;

            variables.forEach( (value) => {
                const types = value.types;

                const columnValues = [];

                types.forEach((t, index) => {
                    let columnCount = 1;
                    if(t.hasOwnProperty('count'))
                    {
                        columnCount = resolveExpression(t.count)|0;
                    }
									
                    let values = new Array(columnCount).fill(0);
						
                    columnValues.push({
                        values: values,
                        type: t.type
                    });                    
                });

                for(let v = 0; v < columnValues.length; v++)
                {
                    const value = columnValues[v];
                    data.push( ...bytesAsSize(value.values, value.type));
                }
            });
        }

		while(exportDataSegments.length)
		{
			const dataSegment = exportDataSegments.shift();
			const dataSegmentOffset = data.length;
			exportDataSegmentOffsets[dataSegment.name] = dataSegmentOffset;
			dataSegment.getBytes(data, exportDataSegments);
		}

		return data;

		function exportSheet(data, sheet, sourceWorksheet, exportDataSegments)
		{
			const sheetName  = sheet.name;
			const dataOffset = 0;

            const countOffsetSegmentName = `${sheetName}Count`;
            const countOffset = 0;

			let rowCount          = 0;
			let rowCapacity       = 0;
			let sourceSheetValues = null;
			let sourceSheetHeader = null;
						
			if(sourceWorksheet)
			{
				sourceSheetValues = XLSX.utils.sheet_to_json(sourceWorksheet, {header: 1, blankrows: false});
				sourceSheetHeader = sourceSheetValues.shift();
				
				const filterColumnIndex = sourceSheetHeader.indexOf('ExportFilter');
				if(filterColumnIndex == -1)
				{
					rowCount    = sourceSheetValues.length;
					rowCapacity = sourceSheetValues.length;
				}
				else
				{
					let count = 0;
					sourceSheetValues.forEach( (row, i) => {
						if(row[filterColumnIndex])
						{
							count += 1;	
						}
					});
					
					rowCount    = count;
					rowCapacity = count;					
				}
			}
			
			if(sheet.hasOwnProperty('capacity'))
			{
				rowCapacity = Math.max(rowCapacity, resolveExpression(sheet.capacity)|0);
			}
			
            relocationTable.push({
                offset: data.length,
                names: [countOffsetSegmentName, sheetName],
                size: schema.meta.size
            });

			data.push( ...bytesAsSize([countOffset, dataOffset], schema.meta.size) );
						
            exportDataSegments.push( {
                name: countOffsetSegmentName,
                getBytes: (data, exportDataSegments) => {
                    data.push( ...bytesAsSize([rowCount], schema.meta.size));
                }
            });                
            
            const columns = sheet.columns;
				
            exportDataSegments.push( {
                name: sheetName,
                getBytes: (data, exportDataSegments) => { 
                    columns.forEach( column => {
                        const columnSegmentName = sheetName + ":" + column.name;
							
                        exportColumn(data, columnSegmentName, rowCapacity, column, sourceSheetValues, sourceSheetHeader, exportDataSegments);
                    });
                }
            });
		}

		function exportColumn(data, columnSegmentName, rowCapacity, column, sourceSheetValues, sourceSheetHeader, exportDataSegments)
		{
			const offset = 0;

            relocationTable.push({
                offset: data.length,
                names: [columnSegmentName],
                size: schema.meta.size
            });

			data.push(...bytesAsSize([offset], schema.meta.size));
			
            const sources = column.sources;
				
            const columnValues = [];
				
            sources.forEach( (source) => {
                let columnCount = 1;
                if(source.hasOwnProperty('count'))
                {
                    columnCount = resolveExpression(source.count)|0;
                }
									
                const elementCapacity = rowCapacity * columnCount;

                let values = new Array(elementCapacity).fill(0);
					
                if(sourceSheetValues)
                {				
                    const sourceColumnName  = undersoreToPascal(source.name);
                    const sourceColumnIndex = sourceSheetHeader.indexOf(sourceColumnName);
                    if(sourceColumnIndex == -1)
                    {
                        console.log(`Column ${sourceColumnName} not found`);
                    }
                    else
                    {
                        const filterColumnIndex = sourceSheetHeader.indexOf('ExportFilter');
                        if(filterColumnIndex == -1)
                        {
                            sourceSheetValues.forEach( (row, i) => {
                                values[i] = row[sourceColumnIndex];
                            });
                        }
                        else
                        {
                            let index = 0;
                            sourceSheetValues.forEach( (row) => {
                                if(row[filterColumnIndex])
                                {
                                    values[index] = row[sourceColumnIndex];
                                    index += 1;	
                                }
                            });						
                        }
                    }
                }
					
                columnValues.push({
                    values: values,
                    type: source.type,
                    count: columnCount
                });
            });
            exportDataSegments.push( {
                name: columnSegmentName,
                getBytes: (data, exportDataSegments) => {
                    if (columnValues.length == 1)
                    {
                        data.push( ...bytesAsSize(columnValues[0].values, columnValues[0].type));
                    }
                    else
                    {							
                        for(let i = 0; i < rowCapacity; i++)
                        {
                            for(let v = 0; v < columnValues.length; v++)
                            {
                                const value = columnValues[v];
                                const index = i * value.count;
                                const slice = value.values.slice(index, index + value.count);
                                data.push( ...bytesAsSize(slice, value.type));
                            }
                        }
                    }
                }
            });
		}
	}
}

function bytesAsSize(values, size)
{
    switch(size)
    {
        case 'uint8_t':
        case 'int8_t':
        {
            return values;
        }
        break;
        case 'uint16_t':
        {
            const u16_buffer = new ArrayBuffer(2 * values.length);
            const u16_array  = new Uint16Array(u16_buffer);
            const byte_array = new Uint8Array(u16_buffer);					
            values.forEach((value, index) => {
                u16_array[index] = value >>> 0;
            });
            return [...byte_array];
        }
        break;
        case 'int16_t':
        {
            const s16_buffer = new ArrayBuffer(2 * values.length);
            const s16_array  = new Int16Array(s16_buffer);
            const byte_array = new Uint8Array(s16_buffer);
            values.forEach((value, index) => {
                s16_array[index] = value+0|0;
            });
					
            return [...byte_array];
        }
        break;
        case 'uint32_t':
        {
            const u32_buffer = new ArrayBuffer(4 * values.length);
            const u32_array  = new Uint32Array(u32_buffer);
            const byte_array = new Uint8Array(u32_buffer);
            values.forEach((value, index) => {
                u32_array[index] = value >>> 0;
            });
					
            return [...byte_array];
        }
        break;
        case 'int32_t':
        {
            const s32_buffer = new ArrayBuffer(4 * values.length);
            const s32_array  = new Int32Array(s32_buffer);
            const byte_array = new Uint8Array(s32_buffer);
            values.forEach((value, index) => {
                s32_array[index] = value+0|0;
            });
					
            return [...byte_array];
        }
        break;
        case 'float':
        {
            const f32_buffer = new ArrayBuffer(4 * values.length);
            const f32_array  = new Float32Array(f32_buffer);
            const byte_array = new Uint8Array(f32_buffer);
            values.forEach((value, index) => {
                f32_array[index] = value;
            });
					
            return [...byte_array];
        }
        break;
        case 'uint64_t':
        {
            const u64_buffer = new ArrayBuffer(8 * values.length);
            const u32_array  = new Uint32Array(u64_buffer);
            const byte_array = new Uint8Array(u64_buffer);
            values.forEach((value, index) => {
                u32_array[(index * 2) + 0] = value >>> 0;
                u32_array[(index * 2) + 1] = 0;
            });
					
            return [...byte_array];
        }
        case 'int64_t':
        {
            const s64_buffer = new ArrayBuffer(8 * values.length);
            const s32_array  = new Int32Array(s64_buffer);
            const byte_array = new Uint8Array(s64_buffer);
            values.forEach((value, index) => {
                s32_array[(index * 2) + 0] = value+0|0;
                s32_array[(index * 2) + 1] = 0;
            });
					
            return [...byte_array];
        }
        break;
        case 'double':
        {
            const f64_buffer = new ArrayBuffer(8 * values.length);
            const f32_array  = new Float32Array(f64_buffer);
            const byte_array = new Uint8Array(f64_buffer);
            values.forEach((value, index) => {
                f32_array[(index * 2) + 0] = value;
                f32_array[(index * 2) + 1] = 0;
            });
					
            return [...byte_array];
        }
        break;
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