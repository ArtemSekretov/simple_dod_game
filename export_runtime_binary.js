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
	const exportDataSegments = [];

	exportSheets(exportDataSegments);
	let data = exportSheets(exportDataSegments);
		
	return data;

	function exportSheets(exportDataSegments)
	{
		const data = [];

		const hasSheets = schema.hasOwnProperty('sheets');

		const hasRootStruct = hasSheets;
		
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
						console.log('Sheet ${sourceWorksheetName} not found');
					}
					else
					{
						sourceWorksheet = sourceWorkbook.Sheets[sourceWorksheetName];
					}
				}
				
				data.push( ...exportSheet(sheet, sourceWorksheet, exportDataSegments) );		
			});
		}

		const reconstructedExportDataSeqments = [];
		while(exportDataSegments.length)
		{
			const dataSegment = exportDataSegments.shift();
			const dataSegmentOffset = data.length;
			dataSegment.offset = dataSegmentOffset;
			data.push( ...dataSegment.getBytes(exportDataSegments));
			reconstructedExportDataSeqments.push(dataSegment);
		}

		exportDataSegments.push( ...reconstructedExportDataSeqments);
		return data;

		function exportSheet(sheet, sourceWorksheet, exportDataSegments)
		{
			const data = [];

			const sheetName = sheet.name;
			const offset    = dataSegmentOffset(exportDataSegments, sheetName);
			const columns   = sheet.columns;

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
				rowCapacity = resolveExpression(sheet.capacity)|0;
			}
			
			data.push( ...bytesAsSize([rowCount, offset], schema.meta.size) );
			
			if(offset == 0)
			{
				exportDataSegments.push( {
					name: sheetName,
					offset: 0,
					getBytes: (exportDataSegments) => {
						const data = [];

						columns.forEach( column => {
							const columnSegmentName = sheetName + ":" + column.name;
							
							data.push( ...exportColumn(columnSegmentName, rowCapacity, column, sourceSheetValues, sourceSheetHeader,  exportDataSegments));
						});
						
						return data;
					}
				});
			}
			return data;
		}

		function exportColumn(columnSegmentName, rowCapacity, column, sourceSheetValues, sourceSheetHeader, exportDataSegments)
		{
			const data       = [];
			const offset     = dataSegmentOffset(exportDataSegments, columnSegmentName);
			const columnType = column.type;

			data.push(...bytesAsSize([offset], schema.meta.size));
			
			let columnCount = 1;

			if(column.hasOwnProperty('count'))
			{
				columnCount = resolveExpression(column.count)|0;
			}

			const elementCapacity = rowCapacity * columnCount;

			let columnValues = new Array(elementCapacity).fill(0);
			
			if(sourceSheetValues)
			{				
				const sourceColumnName  = undersoreToPascal(column.name);
				const sourceColumnIndex = sourceSheetHeader.indexOf(sourceColumnName);
				if(sourceColumnIndex == -1)
				{
					console.log('Column ${sourceColumnName} not found');
				}
				else
				{
					const filterColumnIndex = sourceSheetHeader.indexOf('ExportFilter');
					if(filterColumnIndex == -1)
					{
						sourceSheetValues.forEach( (row, i) => {
							columnValues[i] = row[sourceColumnIndex];
						});
					}
					else
					{
						let index = 0;
						sourceSheetValues.forEach( (row, i) => {
							if(row[filterColumnIndex])
							{
								columnValues[index] = row[sourceColumnIndex];
								index += 1;	
							}
						});						
					}	
				
				}
			}
			
			if(offset == 0)
			{
				exportDataSegments.push( {
					name: columnSegmentName,
					offset: 0,
					getBytes: (exportDataSegments) => {
						return bytesAsSize(columnValues, columnType);
					}
				});
			}
			return data;
		}

		function dataSegmentOffset(exportDataSegments, dataSegmentName)
		{
			const dataSegmentIndex = exportDataSegments.findIndex( dataSegment => dataSegment.name ==  dataSegmentName);
			if(dataSegmentIndex != -1)
			{
				return exportDataSegments[dataSegmentIndex].offset;
			}
			return 0;
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
			}
		}
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